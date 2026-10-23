#pragma once
// Hardware layer: display + LVGL, touch, IMU, PMU and RTC.

#include <stdint.h>
#include <time.h>

namespace buddy {
class SoundSynth;
class MicListener;
}

namespace board {

bool beginDisplay();               // CO5300 + LVGL display
void setBrightness(uint8_t value);
void setRefreshPeriod(uint32_t ms);
uint32_t displayTimeouts();  // diagnostics: DMA buffer was not free in time  // LVGL refresh (33 = 30 fps)
// Since the previous call: frames, total frame time, of which rotating and sending (QSPI)
void takeFrameStats(uint32_t& frames, uint64_t& renderUs, uint64_t& rotateUs, uint64_t& sendUs);

bool beginTouch();
bool readTouch(int& x, int& y);    // true = pressed

bool beginImu();
bool readAccel(float& ax, float& ay, float& az);  // in g, false = no new data

bool beginPmu();
int batteryPercent();              // -1 = no battery
bool isCharging();
bool pwrKeyPressed();              // short press on PWR since the previous call

bool beginRtc();                   // also sets the system time from the RTC
void rtcWrite(time_t t);

bool beginAudio(buddy::SoundSynth& synth, buddy::MicListener& mic);  // ES8311 + I2S, starts an audio task

}  // namespace board
