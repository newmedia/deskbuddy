#include "board.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <esp_timer.h>
#include <sys/time.h>

#define XPOWERS_CHIP_AXP2101
#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>
#include <TouchDrvCSTXXX.hpp>
#include <XPowersLib.h>

#include "pins.h"

namespace board {

namespace {

TouchDrvCST92xx touch;
SensorQMI8658 imu;
XPowersPMU pmu;
SensorPCF85063 rtc;
bool touchOk = false, imuOk = false, pmuOk = false, rtcOk = false;
volatile bool touchIrq = false;
void IRAM_ATTR onTouchIrq() { touchIrq = true; }

// Days since 1970-01-01 (proleptic Gregorian calendar), for UTC <-> time_t without timegm.
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

}  // namespace

bool beginTouch() {
  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW);
  delay(30);
  digitalWrite(TP_RESET, HIGH);
  delay(50);
  touch.setPins(TP_RESET, TP_INT);
  touchOk = touch.begin(Wire, TP_ADDR, I2C_SDA, I2C_SCL);
  if (touchOk) {
    touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
#if LCD_SW_ROTATION
    touch.setMirrorXY(true, true);
#else
    // XL: same mapping as Waveshare's example for MADCTL 0xA0 (same SensorLib); verify on the device
    touch.setSwapXY(true);
    touch.setMirrorXY(true, false);
#endif
    pinMode(TP_INT, INPUT_PULLUP);
    attachInterrupt(TP_INT, onTouchIrq, FALLING);
  }
  return touchOk;
}

bool readTouch(int& x, int& y) {
  if (!touchOk) return false;
  // Only read via I2C when the touch chip signals something (interrupt), right after a touch, or as
  // a safety net every 100 ms. Saves most of the I2C traffic when nobody touches the screen.
  static uint32_t lastActive = 0, lastPoll = 0;
  uint32_t now = millis();
  bool irq = touchIrq;
  if (!irq && now - lastActive > 150 && now - lastPoll < 100) return false;
  touchIrq = false;
  lastPoll = now;
  const TouchPoints& p = touch.getTouchPoints();
  if (p.getPointCount() == 0) return false;  // hasPoints() can be true without a valid point
  lastActive = now;
  int px = p.getPoint(0).x, py = p.getPoint(0).y;
#if LCD_SW_ROTATION
  // Physical -> logical (inverse of ROTATION_270: logical (x,y) -> physical (W-1-y, x))
  x = py;
  y = LCD_WIDTH - 1 - px;
#else
  // The panel rotates in hardware and the touch chip is set up to match: already logical
  x = px;
  y = py;
#endif
  return true;
}

bool beginImu() {
  imuOk = imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
  if (imuOk) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  }
  return imuOk;
}

bool readAccel(float& ax, float& ay, float& az) {
  if (!imuOk || !imu.getDataReady()) return false;
  float x, y;
  if (!imu.getAccelerometer(x, y, az)) return false;
  // IMU axes -> screen axes, measured with tools/imu-measure.py: standing upright, gravity
  // points along IMU-y (= down on the screen), tilting right gives +x, and tilting the top
  // towards you gives +z. XL: not measured yet (depends on how the board sits in the case); same
  // mapping until then.
  ax = x;
  ay = y;
  return true;
}

bool beginPmu() {
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
  if (!pmuOk) return false;
  // Only enable the measurements and the PWR button; voltage rails and charge settings stay as they are.
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  pmu.enableSystemVoltageMeasure();
  pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  pmu.clearIrqStatus();
  pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  return true;
}

int batteryPercent() {
  if (!pmuOk || !pmu.isBatteryConnect()) return -1;
  return pmu.getBatteryPercent();
}

bool isCharging() { return pmuOk && pmu.isCharging(); }

bool pwrKeyPressed() {
  if (!pmuOk) return false;
  pmu.getIrqStatus();
  bool pressed = pmu.isPekeyShortPressIrq();
  pmu.clearIrqStatus();
  return pressed;
}

bool beginRtc() {
  // Dutch time zone; the RTC itself runs on UTC.
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  rtcOk = rtc.begin(Wire, I2C_SDA, I2C_SCL);
  time_t t = 0;
  if (rtcOk && rtc.isClockIntegrityGuaranteed()) {
    RTC_DateTime d = rtc.getDateTime();
    t = (time_t)(daysFromCivil(d.getYear(), d.getMonth(), d.getDay()) * 86400 + d.getHour() * 3600 +
                 d.getMinute() * 60 + d.getSecond());
  }
  if (t < 1700000000) {
    // RTC empty or invalid: start at the build time (local time of the build PC).
    // __DATE__ = "Oct 15 2026", __TIME__ = "23:52:01" (without sscanf: that pulls in ~16 KB of scanf code)
    struct tm b = {};
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char* d = __DATE__;
    const char* tm = __TIME__;
    char mon[4] = {d[0], d[1], d[2], 0};
    b.tm_mon = (int)((strstr(months, mon) - months) / 3);
    b.tm_mday = atoi(d + 4);
    b.tm_year = atoi(d + 7) - 1900;
    b.tm_hour = atoi(tm);
    b.tm_min = atoi(tm + 3);
    b.tm_sec = atoi(tm + 6);
    b.tm_isdst = -1;
    t = mktime(&b);
    if (rtcOk) rtcWrite(t);
  }
  struct timeval tv = {t, 0};
  settimeofday(&tv, nullptr);
  return rtcOk;
}

void rtcWrite(time_t t) {
  if (!rtcOk) return;
  struct tm u;
  gmtime_r(&t, &u);
  rtc.setDateTime(RTC_DateTime(u));
}

}  // namespace board
