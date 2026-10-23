// deskbuddy firmware for the Waveshare ESP32-S3-Touch-AMOLED-1.75.
// Phase 1: procedural face, touch and motion gestures, clock and info panel (offline).

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <lvgl.h>

#include "app.h"
#include "board/board.h"
#include "board/pins.h"
#include "net/ble.h"
#include "net/net.h"
#include "net/updater.h"

#ifndef DESKBUDDY_VERSION
#define DESKBUDDY_VERSION "0.1.0"
#endif

using namespace buddy;

namespace {

Preferences prefs;

// Performance measurement, updated every second in loop()
struct {
  float fps = 0, renderMs = 0, load = 0, rotateMs = 0, sendMs = 0;
  uint64_t busyUs = 0;
  uint32_t windowStart = 0;
} perf;

struct EspPlatform : Platform {
  void loadSettings(Settings& s) override {
    prefs.begin("deskbuddy", false);  // rw: creates the namespace on first start
    s.sound = prefs.getBool("sound", s.sound);
    s.volume = prefs.getUChar("volume", s.volume);
    s.brightness = prefs.getUChar("bright", s.brightness);
    Look l;
    if (prefs.getBytesLength("look") == sizeof(Look) && prefs.getBytes("look", &l, sizeof(Look)) == sizeof(Look) &&
        l.ver == Look().ver) {
      l.name[sizeof(l.name) - 1] = 0;
      s.look = l;
    } else if (prefs.isKey("skin")) {
      s.look = preset(prefs.getUChar("skin", 0) == 1 ? 1 : 0);  // old setting (skin only)
    }
    s.sleepMin = prefs.getUChar("sleep", s.sleepMin);
    s.ble = prefs.getBool("ble", s.ble);
    s.mic = prefs.getBool("mic", s.mic);
    s.lang = prefs.getUChar("lang", s.lang);
    prefs.end();
  }
  size_t loadBlob(const char* key, void* buf, size_t max) override {
    prefs.begin("deskbuddy", false);
    size_t n = prefs.isKey(key) ? prefs.getBytes(key, buf, max) : 0;
    prefs.end();
    return n;
  }
  void saveBlob(const char* key, const void* buf, size_t len) override {
    prefs.begin("deskbuddy", false);
    prefs.putBytes(key, buf, len);
    prefs.end();
  }
  void saveSettings(const Settings& s) override {
    prefs.begin("deskbuddy", false);
    prefs.putBool("sound", s.sound);
    prefs.putUChar("volume", s.volume);
    prefs.putUChar("bright", s.brightness);
    prefs.putBytes("look", &s.look, sizeof(Look));
    prefs.putUChar("sleep", s.sleepMin);
    prefs.putBool("ble", s.ble);
    prefs.putBool("mic", s.mic);
    prefs.putUChar("lang", s.lang);
    prefs.end();
  }
  void localTime(struct tm& out) override {
    time_t t = time(nullptr);
    localtime_r(&t, &out);
  }
  void deviceInfo(DeviceInfo& d) override {
    d.batteryPct = board::batteryPercent();
    d.charging = board::isCharging();
    snprintf(d.version, sizeof(d.version), "%s", DESKBUDDY_VERSION);
    net::fill(d);
    ble::fill(d);
    d.heapFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    d.heapMin = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    d.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    d.fps = perf.fps;
    d.renderMs = perf.renderMs;
    d.load = perf.load;
    d.rotateMs = perf.rotateMs;
    d.sendMs = perf.sendMs;
  }
  void setBrightness(uint8_t v) override { board::setBrightness(v); }
  void startWifiSetup() override { net::startSetupAp(); }
  void reboot() override { ESP.restart(); }
  void setBle(bool on) override { ble::setEnabled(on); }
  void forgetBle() override { ble::forgetBonds(); }
  void wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) override {
    net::action(action, ssid, pass, reply, len);
  }
  void firmwareUpdate(const char* action, char* reply, size_t len) override {
    net::updater::action(action, reply, len);
  }
};

EspPlatform platform;
App app(platform);

char line[96];
size_t lineLen = 0;

void handleSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n' || lineLen == sizeof(line) - 1) {
      line[lineLen] = 0;
      lineLen = 0;
      static char* reply = (char*)heap_caps_malloc(700, MALLOC_CAP_SPIRAM);  // PSRAM: internal RAM is tight
      app.command(line, reply, 700);
      if (reply[0]) Serial.println(reply);
    } else {
      line[lineLen++] = c;
    }
  }
}

}  // namespace

void setup() {
  // Plain malloc/new from 256 bytes up goes to PSRAM (default only from 4 KB). Internal RAM is the
  // scarcest (Wi-Fi + BLE); what really must be internal (DMA, ISR) asks for that itself with heap_caps flags.
  net::updateMemory(false);
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL, 400000);  // fast mode: touch/IMU polling takes less time

  bool pmu = board::beginPmu();
  bool disp = board::beginDisplay();
  bool touch = board::beginTouch();
  bool imu = board::beginImu();
  bool rtc = board::beginRtc();
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  app.create(lv_screen_active());
  app.showSplash();
  Serial.printf("[mem] after display/sensors: %u KB internal free\n", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  bool audio = board::beginAudio(app.sound, app.mic);
  net::updater::begin(DESKBUDDY_VERSION);
  // During an update installation loop() is stalled; this keeps the face and the ring moving.
  // Max ~10 fps during a (blocking) update: the download/upload gets priority over drawing.
  static void (*const pump)() = [] {
    static uint32_t last = millis();
    uint32_t now = millis();
    if (now - last < 100) return;
    board::setRefreshPeriod(33);  // never the slow sleep refresh during an update
    app.update(LV_MIN(now - last, 100u));
    last = now;
    lv_timer_handler();
  };
  net::updater::setPump(pump);
  net::setPump(pump);
  // BLE holds ~64 KB of internal RAM; TLS to GitHub doesn't fit then. Off during an update job,
  // but skip an automatic check while a phone is connected.
  net::updater::setHooks(
      [](bool urgent) {
        if (!urgent && ble::isConnected()) return false;
        ble::suspend();
        return true;
      },
      [] { ble::resume(); });
  net::begin(app);
  {
    uint64_t mac = ESP.getEfuseMac();
    char name[24];
    snprintf(name, sizeof(name), "deskbuddy-%02X%02X", (unsigned)((mac >> 32) & 0xff), (unsigned)((mac >> 40) & 0xff));
    Serial.printf("[mem] after Wi-Fi: %u KB internal free\n", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    ble::begin(app, app.settings().ble, name);
    Serial.printf("[mem] after BLE: %u KB internal free\n", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  }
  Serial.printf("deskbuddy %s  display=%d touch=%d imu=%d pmu=%d rtc=%d audio=%d  psram=%u KB free\n",
                DESKBUDDY_VERSION, disp, touch, imu, pmu, rtc, audio, (unsigned)(ESP.getFreePsram() / 1024));
  static const char* const reasons[] = {"unknown", "power-on", "external", "software", "PANIC", "int-watchdog",
                                        "task-watchdog", "watchdog", "deepsleep", "BROWNOUT", "sdio", "usb",
                                        "jtag", "efuse", "brown-out dip", "cpu-lockup"};
  int rr = (int)esp_reset_reason();
  Serial.printf("reset reason: %s  heap free: %u KB (internal)\n", rr < 16 ? reasons[rr] : "?",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  Serial.println("type 'help' for commands");
}

void loop() {
  static uint32_t last = millis(), lastTouch = 0, lastImu = 0, lastKey = 0;
  static bool bootWas = false;
  static uint8_t bootEmotion = 0;
  uint32_t now = millis();
  uint32_t loopStartUs = micros();
  uint32_t dt = now - last;
  last = now;

  if (now - lastTouch >= 15) {
    lastTouch = now;
    int x = 0, y = 0;
    bool pressed = board::readTouch(x, y);
    app.onTouch(pressed, x, y, now);
  }
  if (now - lastImu >= 20) {
    lastImu = now;
    float ax, ay, az;
    if (board::readAccel(ax, ay, az)) app.onAccel(ax, ay, az, now);
  }
  if (now - lastKey >= 100) {
    lastKey = now;
    if (board::pwrKeyPressed()) {
      if (app.info.visible()) app.info.hide();
      else app.showInfo();
    }
    // BOOT button: cycle through emotions (handy for testing).
    bool boot = digitalRead(BOOT_BUTTON) == LOW;
    if (boot && !bootWas) {
      bootEmotion = (bootEmotion + 1) % (uint8_t)Emotion::Count;
      app.behaviour.command((Emotion)bootEmotion, 3000);
    }
    bootWas = boot;
  }

  handleSerial();
  net::loop();
  ble::loop();
  app.update(dt);
  // Sleepy or asleep: only slow animations (Zzz, eyelids), so ~8-10 fps; awake: 30 fps.
  Emotion e = app.face.emotion();
  // Calm animations don't need 30 fps: asleep ~8, sleepy ~10, listening (ripples) ~15
  const bool calm = !app.face.busy() && !app.behaviour.dancing();
  board::setRefreshPeriod(e == Emotion::Asleep ? 125 : e == Emotion::Sleepy && calm ? 100 : app.face.listening() && calm ? 66 : 33);
  uint32_t idleMs = lv_timer_handler();

  // Load: time this loop was busy (everything except the delay below)
  perf.busyUs += (uint64_t)(micros() - loopStartUs);
  if (now - perf.windowStart >= 1000) {
    uint32_t frames;
    uint64_t renderUs, rotUs, txUs;
    board::takeFrameStats(frames, renderUs, rotUs, txUs);
    perf.rotateMs = frames ? rotUs / 1000.0f / frames : 0;
    perf.sendMs = frames ? txUs / 1000.0f / frames : 0;
    float win = (now - perf.windowStart) / 1000.0f;
    perf.fps = frames / win;
    perf.renderMs = frames ? renderUs / 1000.0f / frames : 0;
    perf.load = LV_MIN(100.0f, perf.busyUs / 10.0f / (now - perf.windowStart));
    perf.busyUs = 0;
    perf.windowStart = now;
  }
  // Really wait until there is work (LVGL timer, touch poll every 15 ms) instead of spinning.
  delay(LV_CLAMP(1u, idleMs, 10u));
}
