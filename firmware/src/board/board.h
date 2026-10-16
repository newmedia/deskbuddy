#pragma once
// Hardwarelaag: display + LVGL, touch, IMU, PMU en RTC.

#include <stdint.h>
#include <time.h>

namespace buddy {
class SoundSynth;
class MicListener;
}

namespace board {

bool beginDisplay();               // CO5300 + LVGL-display
void setBrightness(uint8_t value);
void setRefreshPeriod(uint32_t ms);
uint32_t displayTimeouts();  // diagnose: DMA-buffer kwam niet op tijd vrij  // LVGL-verversing (33 = 30 fps)
// Sinds vorige aanroep: frames, totale frametijd, waarvan draaien en versturen (QSPI)
void takeFrameStats(uint32_t& frames, uint64_t& renderUs, uint64_t& rotateUs, uint64_t& sendUs);

bool beginTouch();
bool readTouch(int& x, int& y);    // true = ingedrukt

bool beginImu();
bool readAccel(float& ax, float& ay, float& az);  // in g, false = geen nieuwe data

bool beginPmu();
int batteryPercent();              // -1 = geen accu
bool isCharging();
bool pwrKeyPressed();              // korte druk op PWR sinds vorige aanroep

bool beginRtc();                   // zet ook de systeemtijd vanuit de RTC
void rtcWrite(time_t t);

bool beginAudio(buddy::SoundSynth& synth, buddy::MicListener& mic);  // ES8311 + I2S, start een audio-task

}  // namespace board
