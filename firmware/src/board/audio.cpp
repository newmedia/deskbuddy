// Speaker: ES8311 codec (I2C config via Wire) + I2S output + amplifier (PA) on GPIO46.
// Microphones: ES7210 ADC (2 mics) on the same I2S bus (data in on GPIO10), read in the same task.
// We deliberately don't use an ES8311 driver on the old IDF I2C API: it clashes with Wire (i2c_master)
// in Arduino-ESP32 3.x. Only the DAC/playback path is configured (16 kHz, MCLK = 256·fs).

#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>

#include "board.h"
#include "mic.h"
#include "sound.h"

namespace board {

namespace {

constexpr uint8_t kAddr = 0x18;
constexpr int kMclk = 42, kBclk = 9, kWs = 45, kDout = 8, kDin = 10, kPa = 46;

I2SClass i2s;
buddy::SoundSynth* synth = nullptr;
buddy::MicListener* mic = nullptr;
bool ok = false;
uint8_t micAddr = 0;  // I2C address of the ES7210 (0 = not found)

bool wr(uint8_t reg, uint8_t val, uint8_t addr = kAddr) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// ES7210 as slave on the shared clocks (MCLK 4.096 MHz, 16 kHz), mic 1 + 2 in standard I2S,
// 16 bit (mic 1 left, mic 2 right). Order follows Espressif's es7210 driver (esp_codec_dev).
bool micInit() {
  for (uint8_t a = 0x40; a <= 0x43 && !micAddr; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) micAddr = a;
  }
  if (!micAddr) return false;
  const uint8_t gain = 0x10 | 0x0A;  // gain on, 30 dB
  static const uint8_t seq[][2] = {
      {0x00, 0xFF}, {0x00, 0x41}, {0x01, 0x3F}, {0x09, 0x30}, {0x0A, 0x30},  // reset, clocks off, timing
      {0x23, 0x2A}, {0x22, 0x0A}, {0x20, 0x0A}, {0x21, 0x2A},                // high-pass filters
      {0x08, 0x10},                                                          // slave mode
      {0x40, 0x43}, {0x41, 0x70}, {0x42, 0x70},                              // analog on, mic bias
      {0x07, 0x20}, {0x02, 0xC1}, {0x04, 0x01}, {0x05, 0x00},                // OSR, clock divider, LRCK = MCLK/256
      {0x11, 0x60}, {0x12, 0x00},                                            // 16 bit I2S, no TDM
      {0x4B, 0xFF}, {0x4C, 0xFF},                                            // all mics off first
      {0x01, 0x34}, {0x4B, 0x00}, {0x43, gain}, {0x44, gain},                // mic 1 + 2 on
      {0x06, 0x00}, {0x47, 0x08}, {0x48, 0x08}, {0x49, 0x08}, {0x4A, 0x08},  // start
      {0x40, 0x43}, {0x00, 0x71}, {0x00, 0x41},
  };
  for (auto& r : seq)
    if (!wr(r[0], r[1], micAddr)) return false;
  return true;
}

bool codecInit(uint8_t volumeReg) {
  // Reset + clocks (based on the register order of Espressif's es8311 driver).
  static const uint8_t seq[][2] = {
      {0x45, 0x00}, {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10}, {0x05, 0x00},
      {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F}, {0x00, 0x80},  // slave mode, on
      {0x01, 0x3F},                                                          // MCLK from pin, all clocks
      // 4.096 MHz / 16 kHz: pre_div 1, x1, dac_div 1, fs single, LRCK 256, BCLK /4, OSR 0x10
      {0x02, 0x00}, {0x05, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x07, 0x00}, {0x08, 0xFF}, {0x06, 0x03},
      {0x09, 0x0C}, {0x0A, 0x0C},                                            // I2S, 16 bit in/out
      // Start up the DAC path
      {0x17, 0xBF}, {0x0E, 0x02}, {0x12, 0x00}, {0x14, 0x1A}, {0x0D, 0x01}, {0x15, 0x40}, {0x37, 0x08},
      {0x45, 0x00},
  };
  for (auto& r : seq)
    if (!wr(r[0], r[1])) return false;
  return wr(0x32, volumeReg);  // DAC volume (0xBF = 0 dB)
}

void audioTask(void*) {
  static int16_t mono[256];
  static int16_t stereo[512];
  static int16_t in[512];
  uint32_t quietSince = millis();
  bool paOn = false;
  for (;;) {
    bool playing = synth->fill(mono, 256);
    for (int i = 0; i < 256; i++) stereo[2 * i] = stereo[2 * i + 1] = mono[i];
    if (playing) {
      quietSince = millis();
      if (!paOn) {
        digitalWrite(kPa, HIGH);
        paOn = true;
      }
    } else if (paOn && millis() - quietSince > 400) {
      digitalWrite(kPa, LOW);  // amplifier off when silent: no noise, less current
      paOn = false;
    }
    i2s.write((uint8_t*)stereo, sizeof(stereo));  // blocks until the DMA has room

    // Microphones: read as many frames as we just wrote (same clock), mix to mono.
    if (micAddr) {
      size_t got = i2s.readBytes((char*)in, sizeof(in));
      if (paOn) mic->hold(300);  // don't "hear" our own sound
      size_t frames = got / 4;
      for (size_t i = 0; i < frames; i++) mono[i] = (int16_t)(((int32_t)in[2 * i] + in[2 * i + 1]) / 2);
      if (frames) mic->feed(mono, frames);
    }
  }
}

}  // namespace

bool beginAudio(buddy::SoundSynth& s, buddy::MicListener& m) {
  synth = &s;
  mic = &m;
  pinMode(kPa, OUTPUT);
  digitalWrite(kPa, LOW);
  i2s.setPins(kBclk, kWs, kDout, kDin, kMclk);
  if (!i2s.begin(I2S_MODE_STD, buddy::SoundSynth::kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
                 I2S_STD_SLOT_BOTH))
    return false;
  if (!codecInit(0xD0)) return false;  // slightly above 0 dB; the synth controls the actual volume
  if (micInit()) Serial.printf("[mic] ES7210 at 0x%02X\n", micAddr);
  else Serial.println("[mic] ES7210 not found");
  ok = xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 0) == pdPASS;
  return ok;
}

}  // namespace board
