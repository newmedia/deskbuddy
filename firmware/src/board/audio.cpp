// Speaker: ES8311-codec (I2C-config via Wire) + I2S-uitvoer + versterker (PA) op GPIO46.
// Microfoons: ES7210-ADC (2 mics) op dezelfde I2S-bus (data in op GPIO10), gelezen in dezelfde task.
// We gebruiken bewust geen ES8311-driver op de oude IDF-I2C-API: die botst met Wire (i2c_master)
// in Arduino-ESP32 3.x. Alleen het DAC/afspeelpad wordt ingesteld (16 kHz, MCLK = 256·fs).

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
uint8_t micAddr = 0;  // I2C-adres van de ES7210 (0 = niet gevonden)

bool wr(uint8_t reg, uint8_t val, uint8_t addr = kAddr) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

// ES7210 als slave op de gedeelde klokken (MCLK 4,096 MHz, 16 kHz), mic 1 + 2 in standaard-I2S,
// 16 bit (mic 1 links, mic 2 rechts). Volgorde naar Espressif's es7210-driver (esp_codec_dev).
bool micInit() {
  for (uint8_t a = 0x40; a <= 0x43 && !micAddr; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) micAddr = a;
  }
  if (!micAddr) return false;
  const uint8_t gain = 0x10 | 0x0A;  // versterking aan, 30 dB
  static const uint8_t seq[][2] = {
      {0x00, 0xFF}, {0x00, 0x41}, {0x01, 0x3F}, {0x09, 0x30}, {0x0A, 0x30},  // reset, klokken uit, timing
      {0x23, 0x2A}, {0x22, 0x0A}, {0x20, 0x0A}, {0x21, 0x2A},                // hoogdoorlaatfilters
      {0x08, 0x10},                                                          // slave-modus
      {0x40, 0x43}, {0x41, 0x70}, {0x42, 0x70},                              // analoog aan, mic-bias
      {0x07, 0x20}, {0x02, 0xC1}, {0x04, 0x01}, {0x05, 0x00},                // OSR, klokdeler, LRCK = MCLK/256
      {0x11, 0x60}, {0x12, 0x00},                                            // 16 bit I2S, geen TDM
      {0x4B, 0xFF}, {0x4C, 0xFF},                                            // alle mics eerst uit
      {0x01, 0x34}, {0x4B, 0x00}, {0x43, gain}, {0x44, gain},                // mic 1 + 2 aan
      {0x06, 0x00}, {0x47, 0x08}, {0x48, 0x08}, {0x49, 0x08}, {0x4A, 0x08},  // starten
      {0x40, 0x43}, {0x00, 0x71}, {0x00, 0x41},
  };
  for (auto& r : seq)
    if (!wr(r[0], r[1], micAddr)) return false;
  return true;
}

bool codecInit(uint8_t volumeReg) {
  // Reset + klokken (gebaseerd op de register-volgorde van Espressif's es8311-driver).
  static const uint8_t seq[][2] = {
      {0x45, 0x00}, {0x01, 0x30}, {0x02, 0x00}, {0x03, 0x10}, {0x16, 0x24}, {0x04, 0x10}, {0x05, 0x00},
      {0x0B, 0x00}, {0x0C, 0x00}, {0x10, 0x1F}, {0x11, 0x7F}, {0x00, 0x80},  // slave-modus, aan
      {0x01, 0x3F},                                                          // MCLK van pin, alle klokken
      // 4.096 MHz / 16 kHz: pre_div 1, x1, dac_div 1, fs single, LRCK 256, BCLK /4, OSR 0x10
      {0x02, 0x00}, {0x05, 0x00}, {0x03, 0x10}, {0x04, 0x10}, {0x07, 0x00}, {0x08, 0xFF}, {0x06, 0x03},
      {0x09, 0x0C}, {0x0A, 0x0C},                                            // I2S, 16 bit in/uit
      // Opstarten DAC-pad
      {0x17, 0xBF}, {0x0E, 0x02}, {0x12, 0x00}, {0x14, 0x1A}, {0x0D, 0x01}, {0x15, 0x40}, {0x37, 0x08},
      {0x45, 0x00},
  };
  for (auto& r : seq)
    if (!wr(r[0], r[1])) return false;
  return wr(0x32, volumeReg);  // DAC-volume (0xBF = 0 dB)
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
      digitalWrite(kPa, LOW);  // versterker uit als het stil is: geen ruis, minder stroom
      paOn = false;
    }
    i2s.write((uint8_t*)stereo, sizeof(stereo));  // blokkeert tot DMA ruimte heeft

    // Microfoons: evenveel frames lezen als we net schreven (zelfde klok), mixen naar mono.
    if (micAddr) {
      size_t got = i2s.readBytes((char*)in, sizeof(in));
      if (paOn) mic->hold(300);  // eigen geluid niet "horen"
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
  if (!codecInit(0xD0)) return false;  // iets boven 0 dB; het eigenlijke volume regelt de synth
  if (micInit()) Serial.printf("[mic] ES7210 op 0x%02X\n", micAddr);
  else Serial.println("[mic] ES7210 niet gevonden");
  ok = xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 0) == pdPASS;
  return ok;
}

}  // namespace board
