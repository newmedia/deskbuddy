// deskbuddy-firmware voor de Waveshare ESP32-S3-Touch-AMOLED-1.75.
// Fase 1: procedureel gezicht, touch- en bewegingsgebaren, klok en info-paneel (offline).

#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_system.h>
#include <lvgl.h>

#include "app.h"
#include "board/board.h"
#include "board/pins.h"

#ifndef DESKBUDDY_VERSION
#define DESKBUDDY_VERSION "0.1.0"
#endif

using namespace buddy;

namespace {

Preferences prefs;

struct EspPlatform : Platform {
  void loadSettings(Settings& s) override {
    prefs.begin("deskbuddy", false);  // rw: maakt de namespace aan bij de eerste start
    s.sound = prefs.getBool("sound", s.sound);
    s.volume = prefs.getUChar("volume", s.volume);
    s.brightness = prefs.getUChar("bright", s.brightness);
    s.skin = (Skin)prefs.getUChar("skin", (uint8_t)s.skin);
    if (s.skin >= Skin::Count) s.skin = Skin::Classic;
    prefs.end();
  }
  void saveSettings(const Settings& s) override {
    prefs.begin("deskbuddy", false);
    prefs.putBool("sound", s.sound);
    prefs.putUChar("volume", s.volume);
    prefs.putUChar("bright", s.brightness);
    prefs.putUChar("skin", (uint8_t)s.skin);
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
    // Wi-Fi en BLE volgen in fase 2/3; tot die tijd toont het info-paneel "niet verbonden".
  }
  void setBrightness(uint8_t v) override { board::setBrightness(v); }
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
      char reply[320];
      app.command(line, reply, sizeof(reply));
      if (reply[0]) Serial.println(reply);
    } else {
      line[lineLen++] = c;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL, 400000);  // fast mode: touch/IMU-polling kost minder tijd

  bool pmu = board::beginPmu();
  bool disp = board::beginDisplay();
  bool touch = board::beginTouch();
  bool imu = board::beginImu();
  bool rtc = board::beginRtc();
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  app.create(lv_screen_active());
  bool audio = board::beginAudio(app.sound);
  Serial.printf("deskbuddy %s  display=%d touch=%d imu=%d pmu=%d rtc=%d audio=%d  psram=%u KB vrij\n",
                DESKBUDDY_VERSION, disp, touch, imu, pmu, rtc, audio, (unsigned)(ESP.getFreePsram() / 1024));
  static const char* const reasons[] = {"onbekend", "aan", "extern", "software", "PANIC", "int-watchdog",
                                        "task-watchdog", "watchdog", "deepsleep", "BROWNOUT", "sdio"};
  int rr = (int)esp_reset_reason();
  Serial.printf("reset-reden: %s\n", rr < 11 ? reasons[rr] : "?");
  Serial.println("typ 'help' voor commando's");
}

void loop() {
  static uint32_t last = millis(), lastTouch = 0, lastImu = 0, lastKey = 0;
  static bool bootWas = false;
  static uint8_t bootEmotion = 0;
  uint32_t now = millis();
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
    // BOOT-knop: emoties doorlopen (handig om te testen).
    bool boot = digitalRead(BOOT_BUTTON) == LOW;
    if (boot && !bootWas) {
      bootEmotion = (bootEmotion + 1) % (uint8_t)Emotion::Count;
      app.behaviour.command((Emotion)bootEmotion, 3000);
    }
    bootWas = boot;
  }

  handleSerial();
  app.update(dt);
  lv_timer_handler();
  delay(2);
}
