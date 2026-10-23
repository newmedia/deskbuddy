#pragma once
// Ties face, behaviour, gestures and overlays together. Platform-specific things (time, brightness,
// network status) come via Platform, so the device and the simulator run the same App.
//
// Navigation:  swipe down = info (Wi-Fi/BLE/battery)  ·  swipe up = clock  ·  tap = close

#include <stdio.h>
#include <time.h>

#include "behaviour.h"
#include "clock_view.h"
#include "commands.h"
#include "face.h"
#include "gestures.h"
#include "i18n.h"
#include "info_view.h"
#include "setup_view.h"
#include "mic.h"
#include "splash_view.h"
#include "sound.h"
#include "update.h"

namespace buddy {

struct Settings {
  bool sound = true;
  uint8_t volume = 70;      // 0..100
  uint8_t brightness = 200; // 0..255
  Look look;                // appearance (base, parts, colors); default preset(0) is set in App::create
  uint8_t sleepMin = 3;     // minutes until asleep; 0 = never
  bool ble = true;          // Bluetooth on
  bool mic = true;          // listen to sound (level only; nothing is recorded)
  uint8_t lang = 0;         // Lang: 0 = English, 1 = Dutch (on-screen text and web app)
};

struct Platform {
  virtual ~Platform() = default;
  virtual void loadSettings(Settings&) {}
  // Small binary blobs (custom profiles); returns the number of bytes read.
  virtual size_t loadBlob(const char* key, void* buf, size_t max) { (void)key; (void)buf; (void)max; return 0; }
  virtual void saveBlob(const char* key, const void* buf, size_t len) { (void)key; (void)buf; (void)len; }
  virtual void saveSettings(const Settings&) {}
  virtual void localTime(struct tm& out) = 0;
  virtual void deviceInfo(DeviceInfo& out) = 0;
  virtual void setBrightness(uint8_t) {}
  virtual void reboot() {}
  virtual void startWifiSetup() {}  // start the setup AP (when not connected yet)
  virtual void setBle(bool) {}
  virtual void forgetBle() {}
  virtual void wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) {
    (void)action; (void)ssid; (void)pass;
    snprintf(reply, len, "wifi not available on this platform");
  }
  // Firmware updates (see update.h); action: status | check | install | auto | notify | off
  virtual void firmwareUpdate(const char* action, char* reply, size_t len) {
    (void)action;
    snprintf(reply, len, "updates not available on this platform");
  }
};

class App : public CommandHost {
 public:
  explicit App(Platform& p) : platform_(p) {}

  void create(lv_obj_t* screen);
  void showSplash(uint32_t ms = 1500);  // startup screen with the logo (device and web emulator)
  void onTouch(bool pressed, int x, int y, uint32_t nowMs);
  void onAccel(float ax, float ay, float az, uint32_t nowMs);
  void onGesture(const GestureEvent& g);
  void update(uint32_t dtMs);
  bool command(const char* line, char* reply, size_t len);
  // Status as JSON for the web API (and BLE).
  void statusJson(char* out, size_t len);
  void showWifiSetup();
  const Settings& settings() const { return settings_; }

  // CommandHost
  void setBrightness(uint8_t v) override;
  void showClock() override;
  void showInfo() override;
  void status(char* out, size_t len) override;
  void setSound(bool on) override;
  void setVolume(uint8_t pct) override;
  void playSound(Sound s) override { sound.play(s); }
  void setSkin(Skin s) override;  // switching base = character Robot or Mascot
  void setLook(const Look& l);
  void setLang(Lang l);
  void setSleep(uint8_t minutes) override;
  void ble(const char* action, char* reply, size_t len) override;
  void wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) override;
  void firmwareUpdate(const char* action, char* reply, size_t len) override;

  Face face;
  Behaviour behaviour{face};
  ClockView clock;
  InfoView info;
  SetupView setup;
  PairView pair;
  SplashView splash;
  SoundSynth sound;
  MicListener mic;
  UpdateView updating;

 private:
  void applyBrightness();
  void save();
  void syncDev();
  bool lookCommand(const char* line, char* reply, size_t len);
  void saveProfiles();
  int findProfile(const char* name) const;
  Look profiles_[kMaxProfiles];
  int profileCount_ = 0;
  void applySleep();

  lv_obj_t* screen_ = nullptr;
  Platform& platform_;
  TouchGestures touch_;
  MotionGestures motion_;
  DeviceInfo dev_;
  Settings settings_;
  Emotion lastEmotion_ = Emotion::Neutral;
  bool muteNextEmotion_ = false;
  bool autoSetupShown_ = false;
  bool wasInstalling_ = false;
  int appliedBrightness_ = -1;
  uint32_t saveIn_ = 0;
  uint32_t rebootIn_ = 0;
  uint32_t infoPoll_ = 0;
};

}  // namespace buddy
