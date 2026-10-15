#pragma once
// Koppelt gezicht, gedrag, gebaren en overlays aan elkaar. Platform-specifieke zaken (tijd,
// helderheid, netwerkstatus) komen via Platform, zodat apparaat en simulator dezelfde App draaien.
//
// Navigatie:  veeg omlaag = info (Wi-Fi/BLE/accu)  ·  veeg omhoog = klok  ·  tik = sluiten

#include <stdio.h>
#include <time.h>

#include "behaviour.h"
#include "clock_view.h"
#include "commands.h"
#include "face.h"
#include "gestures.h"
#include "info_view.h"
#include "setup_view.h"
#include "sound.h"

namespace buddy {

struct Settings {
  bool sound = true;
  uint8_t volume = 70;      // 0..100
  uint8_t brightness = 200; // 0..255
  Skin skin = Skin::Classic;
};

struct Platform {
  virtual ~Platform() = default;
  virtual void loadSettings(Settings&) {}
  virtual void saveSettings(const Settings&) {}
  virtual void localTime(struct tm& out) = 0;
  virtual void deviceInfo(DeviceInfo& out) = 0;
  virtual void setBrightness(uint8_t) {}
  virtual void startWifiSetup() {}  // setup-AP starten (als er nog geen verbinding is)
  virtual void wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) {
    (void)action; (void)ssid; (void)pass;
    snprintf(reply, len, "wifi niet beschikbaar op dit platform");
  }
};

class App : public CommandHost {
 public:
  explicit App(Platform& p) : platform_(p) {}

  void create(lv_obj_t* screen);
  void onTouch(bool pressed, int x, int y, uint32_t nowMs);
  void onAccel(float ax, float ay, float az, uint32_t nowMs);
  void onGesture(const GestureEvent& g);
  void update(uint32_t dtMs);
  bool command(const char* line, char* reply, size_t len);
  // Status als JSON voor de web-API (en later BLE/MQTT).
  void statusJson(char* out, size_t len);
  void showWifiSetup();

  // CommandHost
  void setBrightness(uint8_t v) override;
  void showClock() override;
  void showInfo() override;
  void status(char* out, size_t len) override;
  void setSound(bool on) override;
  void setVolume(uint8_t pct) override;
  void playSound(Sound s) override { sound.play(s); }
  void setSkin(Skin s) override;
  void wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) override;

  Face face;
  Behaviour behaviour{face};
  ClockView clock;
  InfoView info;
  SetupView setup;
  SoundSynth sound;

 private:
  void applyBrightness();
  void save();
  void syncDev();

  Platform& platform_;
  TouchGestures touch_;
  MotionGestures motion_;
  DeviceInfo dev_;
  Settings settings_;
  Emotion lastEmotion_ = Emotion::Neutral;
  bool muteNextEmotion_ = false;
  int appliedBrightness_ = -1;
  uint32_t saveIn_ = 0;
  uint32_t infoPoll_ = 0;
};

}  // namespace buddy
