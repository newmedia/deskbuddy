#include "board.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <sys/time.h>

#define XPOWERS_CHIP_AXP2101
#include <SensorPCF85063.hpp>
#include <SensorQMI8658.hpp>
#include <TouchDrvCSTXXX.hpp>
#include <XPowersLib.h>

#include "pins.h"

namespace board {

namespace {

Arduino_DataBus* bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
// Kolom-offset 6: het CO5300-panel van dit bord begint niet op kolom 0 (zie Waveshare-voorbeelden).
Arduino_CO5300* gfx = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

TouchDrvCST92xx touch;
SensorQMI8658 imu;
XPowersPMU pmu;
SensorPCF85063 rtc;
bool touchOk = false, imuOk = false, pmuOk = false, rtcOk = false;

constexpr int kBufLines = 80;
// Het bord zit 90° gedraaid in de behuizing: de fysieke bovenkant van het panel wijst naar links.
// ROTATION_270 legt de logische bovenkant op de fysieke rechterkant, zodat het beeld rechtop staat.
constexpr lv_display_rotation_t kRotation = LV_DISPLAY_ROTATION_270;
uint8_t* rotBuf = nullptr;

uint32_t tickCb() { return millis(); }

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  lv_area_t a = *area;
  if (kRotation != LV_DISPLAY_ROTATION_0) {
    const lv_color_format_t cf = LV_COLOR_FORMAT_RGB565;
    int32_t w = lv_area_get_width(area), h = lv_area_get_height(area);
    lv_display_rotate_area(disp, &a);
    lv_draw_sw_rotate(px, rotBuf, w, h, lv_draw_buf_width_to_stride(w, cf),
                      lv_draw_buf_width_to_stride(lv_area_get_width(&a), cf), kRotation, cf);
    px = rotBuf;
  }
  gfx->draw16bitRGBBitmap(a.x1, a.y1, (uint16_t*)px, a.x2 - a.x1 + 1, a.y2 - a.y1 + 1);
  lv_display_flush_ready(disp);
}

// De CO5300 accepteert alleen vensters met even start- en oneven eindcoördinaten.
void rounderCb(lv_event_t* e) {
  auto* a = (lv_area_t*)lv_event_get_param(e);
  a->x1 &= ~1;
  a->y1 &= ~1;
  a->x2 |= 1;
  a->y2 |= 1;
}

// Dagen sinds 1970-01-01 (proleptische Gregoriaanse kalender), voor UTC <-> time_t zonder timegm.
int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

}  // namespace

bool beginDisplay() {
  if (!gfx->begin()) return false;
  gfx->fillScreen(RGB565_BLACK);
  gfx->setBrightness(200);

  lv_init();
  lv_tick_set_cb(tickCb);
  lv_display_t* disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
  size_t bytes = LCD_WIDTH * kBufLines * sizeof(uint16_t);
  void* buf = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!buf) buf = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!buf) return false;
  rotBuf = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!rotBuf) rotBuf = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!rotBuf) return false;
  lv_display_set_rotation(disp, kRotation);
  lv_display_set_buffers(disp, buf, nullptr, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_add_event_cb(disp, rounderCb, LV_EVENT_INVALIDATE_AREA, nullptr);
  return true;
}

void setBrightness(uint8_t v) { gfx->setBrightness(v); }

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
    touch.setMirrorXY(true, true);
  }
  return touchOk;
}

bool readTouch(int& x, int& y) {
  if (!touchOk) return false;
  const TouchPoints& p = touch.getTouchPoints();
  if (p.getPointCount() == 0) return false;  // hasPoints() kan true zijn zonder geldig punt
  int px = p.getPoint(0).x, py = p.getPoint(0).y;
  // Fysiek -> logisch (inverse van ROTATION_270: logisch (x,y) -> fysiek (W-1-y, x))
  x = py;
  y = LCD_WIDTH - 1 - px;
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
  // IMU-assen -> schermassen, gemeten met tools/imu-measure.py: rechtop staand wijst de
  // zwaartekracht langs IMU-y (= omlaag op het scherm), rechts kantelen geeft +x, en de bovenkant
  // naar je toe kantelen geeft +z.
  ax = x;
  ay = y;
  return true;
}

bool beginPmu() {
  pmuOk = pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL);
  if (!pmuOk) return false;
  // Alleen metingen en de PWR-knop aanzetten; spanningsrails en laadinstellingen blijven zoals ze zijn.
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
  // Nederlandse tijdzone; de RTC zelf loopt op UTC.
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
    // RTC leeg of ongeldig: begin bij de bouwtijd (lokale tijd van de build-PC).
    struct tm b = {};
    char mon[4];
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    sscanf(__DATE__, "%3s %d %d", mon, &b.tm_mday, &b.tm_year);
    sscanf(__TIME__, "%d:%d:%d", &b.tm_hour, &b.tm_min, &b.tm_sec);
    b.tm_mon = (int)((strstr(months, mon) - months) / 3);
    b.tm_year -= 1900;
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
