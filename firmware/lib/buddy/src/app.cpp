#include "app.h"

#include <stdio.h>
#include <string.h>

namespace buddy {

namespace {
constexpr float kHalf = Face::kSize / 2.0f;
}

void App::create(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  face.create(screen);
  clock.create(screen);
  info.create(screen);
  setup.create(screen);

  platform_.loadSettings(settings_);
  sound.setEnabled(settings_.sound);
  sound.setVolume(settings_.volume);
  face.setSkin(settings_.skin);
  applySleep();
  platform_.deviceInfo(dev_);
  syncDev();
  applyBrightness();
}

void App::onTouch(bool pressed, int x, int y, uint32_t now) {
  GestureEvent g = touch_.feed(pressed, x, y, now);
  if (g.type != Gesture::None) onGesture(g);
}

void App::onAccel(float ax, float ay, float az, uint32_t now) {
  GestureEvent g = motion_.feed(ax, ay, az, now);
  if (g.type != Gesture::None) onGesture(g);
  if (!clock.visible() && !info.visible()) behaviour.onGravity(motion_.tiltX(), motion_.tiltY(), 0);
}

void App::onGesture(const GestureEvent& g) {
  // Overlay open: tik of terugvegen sluit hem. In het info-paneel schakelt een tik op "Geluid".
  if (setup.visible()) {
    if (g.type == Gesture::Tap || g.type == Gesture::SwipeUp || g.type == Gesture::SwipeDown) setup.hide();
    return;
  }
  if (info.visible()) {
    InfoView::Hit h = g.type == Gesture::Tap ? info.hit((int)(g.x * kHalf + kHalf), (int)(g.y * kHalf + kHalf))
                                              : InfoView::Hit::None;
    if (h == InfoView::Hit::Wifi) {
      if (dev_.wifi != DeviceInfo::WifiMode::Station) {
        info.hide();
        showWifiSetup();
      }
    } else if (h == InfoView::Hit::Sound) {
      setSound(!settings_.sound);
      info.show();  // auto-sluiten opnieuw starten
    } else if (h == InfoView::Hit::Skin) {
      setSkin(settings_.skin == Skin::Mascot ? Skin::Classic : Skin::Mascot);
      info.show();
    } else if (g.type == Gesture::Tap || g.type == Gesture::SwipeUp) {
      info.hide();
    }
    return;
  }
  if (clock.visible()) {
    if (g.type == Gesture::Tap || g.type == Gesture::SwipeDown) clock.hide();
    return;
  }
  switch (g.type) {
    case Gesture::SwipeDown:
      showInfo();
      break;
    case Gesture::SwipeUp:
      showClock();
      break;
    default: {
      Emotion before = face.emotion();
      behaviour.onGesture(g);
      if (g.type == Gesture::Pet) {
        sound.play(Sound::Purr);
        muteNextEmotion_ = true;
      } else if (g.type == Gesture::Tap && face.emotion() == before) {
        sound.play(Sound::Blip);
      }
      break;
    }
  }
}

void App::showClock() {
  info.hide();
  setup.hide();
  clock.show();
}

void App::showInfo() {
  clock.hide();
  platform_.deviceInfo(dev_);
  syncDev();
  info.show();
}

void App::showWifiSetup() {
  if (dev_.wifi != DeviceInfo::WifiMode::AccessPoint) platform_.startWifiSetup();
  platform_.deviceInfo(dev_);
  syncDev();
  if (dev_.wifi == DeviceInfo::WifiMode::AccessPoint) setup.show(dev_.ssid, dev_.ip);
}

void App::wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) {
  if (!strcmp(action, "setup")) {
    showWifiSetup();
    snprintf(reply, len, "ok setup-AP %s op %s", dev_.ssid, dev_.ip);
    return;
  }
  platform_.wifi(action, ssid, pass, reply, len);
}

void App::statusJson(char* out, size_t len) {
  const char* wifi = "off";
  switch (dev_.wifi) {
    case DeviceInfo::WifiMode::Station: wifi = "sta"; break;
    case DeviceInfo::WifiMode::AccessPoint: wifi = "ap"; break;
    case DeviceInfo::WifiMode::Connecting: wifi = "connecting"; break;
    default: break;
  }
  snprintf(out, len,
           "{\"version\":\"%s\",\"emotion\":\"%s\",\"skin\":\"%s\",\"sound\":%s,\"volume\":%u,"
           "\"brightness\":%u,\"battery\":%d,\"charging\":%s,\"wifi\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\","
           "\"rssi\":%d,\"hostname\":\"%s\",\"idle\":%lu,\"sleep\":%u}",
           dev_.version, emotionName(face.emotion()), skinName(settings_.skin), settings_.sound ? "true" : "false",
           settings_.volume, settings_.brightness, dev_.batteryPct, dev_.charging ? "true" : "false", wifi, dev_.ssid,
           dev_.ip, dev_.rssi, dev_.hostname, (unsigned long)(behaviour.idleMs() / 1000), settings_.sleepMin);
}

void App::update(uint32_t dtMs) {
  behaviour.update(dtMs);
  face.update(dtMs);

  // Geluid bij een nieuwe emotie; wakker worden krijgt een eigen geluidje.
  Emotion e = face.emotion();
  if (e != lastEmotion_) {
    bool woke = (lastEmotion_ == Emotion::Asleep || lastEmotion_ == Emotion::Sleepy) && e != Emotion::Asleep &&
                e != Emotion::Sleepy;
    if (muteNextEmotion_) muteNextEmotion_ = false;
    else if (woke) sound.play(Sound::Wake);
    else if (Sound s = soundForEmotion(e); s != Sound::None) sound.play(s);
    lastEmotion_ = e;
  }

  // Netwerk-/accustatus ~2x per seconde verversen (goedkoop, en het info-paneel blijft actueel).
  infoPoll_ += dtMs;
  if (infoPoll_ >= 500) {
    infoPoll_ = 0;
    platform_.deviceInfo(dev_);
    syncDev();
  }
  struct tm now;
  platform_.localTime(now);
  clock.update(now, dev_.batteryPct, dev_.charging, dtMs);
  info.update(dev_, dtMs);
  setup.update(dtMs);
  if (setup.visible() && dev_.wifi == DeviceInfo::WifiMode::Station) setup.hide();  // gelukt!
  if (setup.visible() && !setup.linkStep() && dev_.apClients > 0) setup.showLinkStep();
  // Telefoon verbindt met het setup-AP zonder dat het setupscherm open is: meteen stap 2 tonen.
  if (dev_.wifi == DeviceInfo::WifiMode::AccessPoint && dev_.apClients > 0) {
    if (!autoSetupShown_ && !setup.visible()) {
      info.hide();
      clock.hide();
      setup.show(dev_.ssid, dev_.ip);
      setup.showLinkStep();
    }
    autoSetupShown_ = true;
  } else if (dev_.apClients == 0) {
    autoSetupShown_ = false;
  }
  applyBrightness();

  // Instellingen pas na een korte pauze opslaan (spaart flash bij snel schuiven/tikken).
  if (saveIn_) {
    if (dtMs >= saveIn_) {
      saveIn_ = 0;
      platform_.saveSettings(settings_);
    } else {
      saveIn_ -= dtMs;
    }
  }
}

void App::save() { saveIn_ = 1500; }

void App::applySleep() {
  if (settings_.sleepMin == 0) {
    behaviour.setSleepAfter(UINT32_MAX, UINT32_MAX);
  } else {
    uint32_t asleep = settings_.sleepMin * 60000u;
    behaviour.setSleepAfter(asleep / 2, asleep);  // halverwege eerst slaperig
  }
}

void App::setSleep(uint8_t minutes) {
  settings_.sleepMin = minutes;
  applySleep();
  save();
}

void App::syncDev() {
  dev_.sound = settings_.sound;
  dev_.mascot = settings_.skin == Skin::Mascot;
}

void App::setSkin(Skin s) {
  settings_.skin = s;
  face.setSkin(s);
  syncDev();
  sound.play(Sound::Toggle);
  save();
}

void App::applyBrightness() {
  int b = behaviour.asleep() ? settings_.brightness / 5 : settings_.brightness;
  if (b != appliedBrightness_) {
    appliedBrightness_ = b;
    platform_.setBrightness((uint8_t)b);
  }
}

void App::setBrightness(uint8_t v) {
  settings_.brightness = v;
  applyBrightness();
  save();
}

void App::setSound(bool on) {
  settings_.sound = on;
  sound.setEnabled(on);
  syncDev();
  sound.play(Sound::Toggle);  // klikje speelt ook bij uitzetten, als bevestiging
  save();
}

void App::setVolume(uint8_t pct) {
  settings_.volume = pct > 100 ? 100 : pct;
  sound.setVolume(settings_.volume);
  sound.play(Sound::Blip);
  save();
}

void App::status(char* out, size_t len) {
  const char* wifi = "off";
  switch (dev_.wifi) {
    case DeviceInfo::WifiMode::Station: wifi = "sta"; break;
    case DeviceInfo::WifiMode::AccessPoint: wifi = "ap"; break;
    case DeviceInfo::WifiMode::Connecting: wifi = "connecting"; break;
    default: break;
  }
  int n = snprintf(out, len, "g=%.2f,%.2f,%.2f tilt=%.2f,%.2f ", motion_.gx(), motion_.gy(), motion_.gz(),
                   motion_.tiltX(), motion_.tiltY());
  if (n < 0 || (size_t)n >= len) return;
  out += n;
  len -= n;
  snprintf(out, len, "skin=%s brightness=%u sound=%s volume=%u battery=%d%s wifi=%s ip=%s ble=%s",
           skinName(settings_.skin), settings_.brightness,
           settings_.sound ? "on" : "off", settings_.volume, dev_.batteryPct, dev_.charging ? "+" : "", wifi,
           dev_.ip[0] ? dev_.ip : "-", dev_.bleEnabled ? (dev_.bleConnected ? "connected" : "on") : "off");
}

bool App::command(const char* line, char* reply, size_t len) {
  return runCommand(line, face, behaviour, *this, reply, len);
}

}  // namespace buddy
