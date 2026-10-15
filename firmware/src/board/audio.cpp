// Speaker: ES8311-codec (I2C-config via Wire) + I2S-uitvoer + versterker (PA) op GPIO46.
// We gebruiken bewust geen ES8311-driver op de oude IDF-I2C-API: die botst met Wire (i2c_master)
// in Arduino-ESP32 3.x. Alleen het DAC/afspeelpad wordt ingesteld (16 kHz, MCLK = 256·fs).

#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>

#include "board.h"
#include "sound.h"

namespace board {

namespace {

constexpr uint8_t kAddr = 0x18;
constexpr int kMclk = 42, kBclk = 9, kWs = 45, kDout = 8, kDin = 10, kPa = 46;

I2SClass i2s;
buddy::SoundSynth* synth = nullptr;
bool ok = false;

bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(kAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
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
  }
}

}  // namespace

bool beginAudio(buddy::SoundSynth& s) {
  synth = &s;
  pinMode(kPa, OUTPUT);
  digitalWrite(kPa, LOW);
  i2s.setPins(kBclk, kWs, kDout, kDin, kMclk);
  if (!i2s.begin(I2S_MODE_STD, buddy::SoundSynth::kRate, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO,
                 I2S_STD_SLOT_BOTH))
    return false;
  if (!codecInit(0xD0)) return false;  // iets boven 0 dB; het eigenlijke volume regelt de synth
  ok = xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, nullptr, 0) == pdPASS;
  return ok;
}

}  // namespace board
