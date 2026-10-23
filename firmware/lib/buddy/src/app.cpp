#include "app.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"

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
  updating.create(screen);
  pair.create(screen);
  screen_ = screen;

  platform_.loadSettings(settings_);
  gLang = settings_.lang == (uint8_t)Lang::Nl ? Lang::Nl : Lang::En;
  {
    uint8_t buf[kPackedLookSize * kMaxProfiles];
    size_t n = platform_.loadBlob("profiles", buf, sizeof(buf));
    profileCount_ = 0;
    for (size_t i = 0; i + kPackedLookSize <= n && profileCount_ < kMaxProfiles; i += kPackedLookSize)
      if (unpackLook(buf + i, profiles_[profileCount_])) profileCount_++;
  }
  sound.setEnabled(settings_.sound);
  mic.setEnabled(settings_.mic);
  sound.setVolume(settings_.volume);
  face.setLook(settings_.look);
  applySleep();
  platform_.deviceInfo(dev_);
  syncDev();
  applyBrightness();
}

void App::showSplash(uint32_t ms) {
  if (!splash.visible()) splash.create(screen_, dev_.version);  // created last: sits above all other layers
  splash.show(ms);
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
  // Overlay open: a tap or swiping back closes it. In the info panel a tap on "Sound" toggles it.
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
    } else if (h == InfoView::Hit::Ble) {
      settings_.ble = !settings_.ble;
      platform_.setBle(settings_.ble);
      sound.play(Sound::Toggle);
      save();
      platform_.deviceInfo(dev_);
      syncDev();
      info.show();
    } else if (h == InfoView::Hit::Sound) {
      setSound(!settings_.sound);
      info.show();  // restart auto-close
    } else if (h == InfoView::Hit::Update) {
      char reply[96];
      firmwareUpdate("install", reply, sizeof(reply));
    } else if (h == InfoView::Hit::Skin) {
      // Next character (custom look -> first character)
      int total = presetCount() + profileCount_;
      int cur = findProfile(settings_.look.name);
      int i = cur >= 0 ? presetCount() + cur : presetIndex(settings_.look.name);
      int next = (i < 0 ? 0 : i + 1) % total;
      setLook(next < presetCount() ? preset(next) : profiles_[next - presetCount()]);
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
    snprintf(reply, len, "ok setup AP %s on %s", dev_.ssid, dev_.ip);
    return;
  }
  platform_.wifi(action, ssid, pass, reply, len);
}

void App::firmwareUpdate(const char* action, char* reply, size_t len) {
  platform_.firmwareUpdate(action, reply, len);
  platform_.deviceInfo(dev_);
  syncDev();
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
           "\"rssi\":%d,\"hostname\":\"%s\",\"idle\":%lu,\"sleep\":%u,\"updates\":\"%s\",\"update\":\"%s\","
           "\"latest\":\"%s\",\"progress\":%u,\"mic\":%s,\"ble\":%s,\"bleConnected\":%s,\"look\":\"%s\",\"lang\":\"%s\",\"heap\":%lu,\"heapMin\":%lu,\"psram\":%lu,\"fps\":%.1f,\"renderMs\":%.1f,\"load\":%.0f}",
           dev_.version, emotionName(face.emotion()), skinName(face.skin()), settings_.sound ? "true" : "false",
           settings_.volume, settings_.brightness, dev_.batteryPct, dev_.charging ? "true" : "false", wifi, dev_.ssid,
           dev_.ip, dev_.rssi, dev_.hostname, (unsigned long)(behaviour.idleMs() / 1000), settings_.sleepMin,
           updateModeName(dev_.updateMode), updateStateName(dev_.updateState), dev_.updateVersion,
           (unsigned)dev_.updateProgress, settings_.mic ? "true" : "false", settings_.ble ? "true" : "false", dev_.bleConnected ? "true" : "false", settings_.look.name, langName(gLang),
           (unsigned long)dev_.heapFree, (unsigned long)dev_.heapMin, (unsigned long)dev_.psramFree, dev_.fps,
           dev_.renderMs, dev_.load);
}

void App::update(uint32_t dtMs) {
  behaviour.update(dtMs);
  // Microphone: events go to the behaviour; while dancing the equalizer mouth moves along
  for (SoundEvent se; (se = mic.poll()) != SoundEvent::None;) behaviour.onSound(se);
  face.setSoundLevel(behaviour.dancing() ? LV_MAX(sound.level(), mic.level()) : sound.level());
  face.update(dtMs);

  // Sound on a new emotion; waking up gets its own sound.
  Emotion e = face.emotion();
  if (e != lastEmotion_) {
    bool woke = (lastEmotion_ == Emotion::Asleep || lastEmotion_ == Emotion::Sleepy) && e != Emotion::Asleep &&
                e != Emotion::Sleepy;
    if (face.busy()) {
      // during an action the action itself decides the sound (via cues)
    } else if (muteNextEmotion_) muteNextEmotion_ = false;
    else if (woke) sound.play(Sound::Wake);
    else if (Sound s = soundForEmotion(e); s != Sound::None) sound.play(s);
    lastEmotion_ = e;
  }

  switch (face.takeCue()) {
    case Cue::Whistle: sound.play(Sound::Whistle); break;
    case Cue::Wink: sound.play(Sound::Wink); break;
    case Cue::Kiss: sound.play(Sound::Kiss); break;
    case Cue::Rage: sound.play(Sound::Rage); break;
    case Cue::Fuse: sound.play(Sound::Fuse); break;
    case Cue::Boom: sound.play(Sound::Boom); break;
    case Cue::Inhale: sound.play(Sound::Inhale); break;
    case Cue::Achoo: sound.play(Sound::Achoo); break;
    case Cue::Laugh: sound.play(Sound::Laugh); break;
    case Cue::Sigh: sound.play(Sound::Sigh); break;
    case Cue::Hiccup: sound.play(Sound::Hiccup); break;
    default: break;
  }

  // Refresh network/battery status ~2x per second (cheap, and keeps the info panel current).
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
  // Show the Bluetooth pairing code as long as the phone asks for it
  if (dev_.blePasskey && !pair.visible()) {
    info.hide();
    clock.hide();
    pair.show(dev_.blePasskey);
  } else if (!dev_.blePasskey && pair.visible()) {
    pair.hide();
  }
  if (setup.visible() && dev_.wifi == DeviceInfo::WifiMode::Station) setup.hide();  // success!
  if (setup.visible() && !setup.linkStep() && dev_.apClients > 0) setup.showLinkStep();
  // Phone joins the setup AP while the setup screen is not open: go straight to step 2.
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
  // Firmware update: surprised face and progress ring; failed = briefly sad.
  bool installing = dev_.updateState == UpdateState::Installing;
  if (installing && !wasInstalling_) {
    // Wake up: sleep mode (slow refresh, dimmed) and an update don't mix, and this way the Matrix
    // animation shows at normal brightness.
    behaviour.command(Emotion::Neutral, 0);
    info.hide();
    clock.hide();
    setup.hide();
    pair.hide();
  } else if (!installing && wasInstalling_) {
    behaviour.command(dev_.updateState == UpdateState::Failed ? Emotion::Sad : Emotion::Neutral, 4000);
  }
  wasInstalling_ = installing;
  updating.update(installing, dev_.updateProgress, dev_.updateVersion);
  face.setMatrix(installing);
  splash.update();
  face.setCovered(!installing && (clock.visible() || info.visible() || setup.visible() || pair.visible() || splash.visible()));
  applyBrightness();

  if (rebootIn_) {
    if (dtMs >= rebootIn_) {
      rebootIn_ = 0;  // once: otherwise the emulator pushes the restart back every frame
      platform_.saveSettings(settings_);
      platform_.reboot();
    } else {
      rebootIn_ -= dtMs;
    }
  }

  // Save settings only after a short pause (spares the flash during quick sliding/tapping).
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
    behaviour.setSleepAfter(asleep / 2, asleep);  // sleepy halfway first
  }
}

void App::setSleep(uint8_t minutes) {
  settings_.sleepMin = minutes;
  applySleep();
  save();
}

void App::syncDev() {
  dev_.sound = settings_.sound;
  dev_.mascot = settings_.look.base == 1;
  snprintf(dev_.lookName, sizeof(dev_.lookName), "%s", settings_.look.name);
}

void App::setSkin(Skin s) { setLook(preset(s == Skin::Mascot ? 1 : 0)); }

void App::setLook(const Look& l) {
  settings_.look = l;
  face.setLook(l);
  syncDev();
  sound.play(Sound::Toggle);
  save();
}

// look                         -> current look as JSON
// look preset <name|number>    -> pick a character (look presets = list)
// look random                  -> surprise
// look <part> <style>          -> e.g. look eyes ring, look hair curly, look base cartoon
// look color <primary|accent|face|frame> <#rrggbb>
// look emotioncolors on|off  ·  look name <name>
int App::findProfile(const char* name) const {
  for (int i = 0; i < profileCount_; i++)
    if (!strcasecmp(profiles_[i].name, name)) return i;
  return -1;
}

void App::saveProfiles() {
  uint8_t buf[kPackedLookSize * kMaxProfiles];
  for (int i = 0; i < profileCount_; i++) packLook(profiles_[i], buf + i * kPackedLookSize);
  platform_.saveBlob("profiles", buf, (size_t)profileCount_ * kPackedLookSize);
}

bool App::lookCommand(const char* line, char* reply, size_t len) {
  char a[16] = "", b[24] = "", c[24] = "";
  sscanf(line, "look %15s %23s %23s", a, b, c);
  Look l = settings_.look;
  if (!a[0]) {
    lookJson(l, reply, len);
    return true;
  }
  // name = rest of the line after the word (may contain spaces)
  auto restAfter = [&](const char* word) {
    const char* p = strstr(line, word);
    p = p ? p + strlen(word) : "";
    while (*p == ' ') p++;
    return p;
  };
  if (!strcmp(a, "presets")) {
    size_t n = snprintf(reply, len, "{\"presets\":[");
    for (int i = 0; i < presetCount() && n < len; i++)
      n += snprintf(reply + n, len - n, "%s\"%s\"", i ? "," : "", preset(i).name);
    if (n < len) n += snprintf(reply + n, len - n, "],\"profiles\":[");
    for (int i = 0; i < profileCount_ && n < len; i++)
      n += snprintf(reply + n, len - n, "%s\"%s\"", i ? "," : "", profiles_[i].name);
    if (n < len) snprintf(reply + n, len - n, "]}");
    return true;
  }
  if (!strcmp(a, "save")) {
    const char* nm = restAfter("save");
    if (!*nm) {
      snprintf(reply, len, "usage: look save <name>");
      return false;
    }
    snprintf(l.name, 13, "%s", nm);  // max 12 characters (compact storage)
    int i = findProfile(l.name);
    if (i < 0) {
      if (profileCount_ >= kMaxProfiles) {
        snprintf(reply, len, "at most %d custom profiles; delete one first", kMaxProfiles);
        return false;
      }
      i = profileCount_++;
    }
    profiles_[i] = l;
    saveProfiles();
  } else if (!strcmp(a, "delete")) {
    int i = findProfile(restAfter("delete"));
    if (i < 0) {
      snprintf(reply, len, "no custom profile with that name");
      return false;
    }
    for (int k = i; k < profileCount_ - 1; k++) profiles_[k] = profiles_[k + 1];
    profileCount_--;
    saveProfiles();
    snprintf(reply, len, "ok deleted");
    return true;
  } else if (!strcmp(a, "preset")) {
    const char* nm = restAfter("preset");
    int i = findProfile(nm);
    if (i >= 0) {
      l = profiles_[i];
    } else if ((i = presetIndex(nm)) >= 0) {
      l = preset(i);
    } else {
      snprintf(reply, len, "unknown character (see 'look presets')");
      return false;
    }
  } else if (!strcmp(a, "random")) {
    l = randomLook((uint32_t)lv_tick_get() * 2654435761u);
  } else if (!strcmp(a, "base")) {
    if (strcmp(b, "robot") && strcmp(b, "cartoon")) {
      snprintf(reply, len, "usage: look base robot|cartoon");
      return false;
    }
    uint8_t nb = !strcmp(b, "cartoon");
    if (nb != l.base) l = preset(nb);  // new base: start from its default character
  } else if (!strcmp(a, "color")) {
    uint32_t col;
    if (!parseColor(c, col)) {
      snprintf(reply, len, "usage: look color primary|accent|face|frame|lip|extra #rrggbb");
      return false;
    }
    if (!strcmp(b, "primary")) l.primary = col;
    else if (!strcmp(b, "accent")) l.accent = col;
    else if (!strcmp(b, "face")) l.face = col;
    else if (!strcmp(b, "frame")) l.frame = col;
    else if (!strcmp(b, "lip")) l.lip = col;
    else if (!strcmp(b, "extra")) l.extra = col;
    else {
      snprintf(reply, len, "unknown color: %s", b);
      return false;
    }
    snprintf(l.name, sizeof(l.name), "%s", tr("Custom", "Eigen"));
  } else if (!strcmp(a, "emotioncolors")) {
    l.emotionColors = !strcmp(b, "on");
  } else if (!strcmp(a, "name")) {
    const char* p = strstr(line, "name");
    p = p ? p + 4 : "";
    while (*p == ' ') p++;
    snprintf(l.name, sizeof(l.name), "%s", p);
  } else {
    Part part;
    uint8_t style;
    if (!partFromName(a, part) || !styleFromName(part, b, style)) {
      snprintf(reply, len, "usage: look [presets|preset <name>|random|base|<part> <style>|color ...]");
      return false;
    }
    uint8_t ok[16];
    int n = stylesFor(part, l.base, ok, 16);
    bool allowed = false;
    for (int i = 0; i < n; i++) allowed |= ok[i] == style;
    if (!allowed) {
      snprintf(reply, len, "%s '%s' does not exist for %s", partName(part), b, l.base ? "cartoon" : "robot");
      return false;
    }
    uint8_t* f[] = {&l.eyes, &l.brows, &l.mouth, &l.cheeks, &l.glasses, &l.hair, &l.lashes, &l.lips, &l.earrings, &l.accessory};
    *f[(int)part] = style;
    snprintf(l.name, sizeof(l.name), "%s", tr("Custom", "Eigen"));
  }
  setLook(l);
  lookJson(l, reply, len);
  return true;
}

void App::applyBrightness() {
  int b = behaviour.asleep() ? settings_.brightness / 5 : settings_.brightness;
  if (b != appliedBrightness_) {
    appliedBrightness_ = b;
    platform_.setBrightness((uint8_t)b);
  }
}

void App::setLang(Lang l) {
  gLang = l;  // the views pick up the new language on their next update
  settings_.lang = (uint8_t)l;
  save();
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
  sound.play(Sound::Toggle);  // the click also plays when switching off, as confirmation
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
           skinName(face.skin()), settings_.brightness,
           settings_.sound ? "on" : "off", settings_.volume, dev_.batteryPct, dev_.charging ? "+" : "", wifi,
           dev_.ip[0] ? dev_.ip : "-", dev_.bleEnabled ? (dev_.bleConnected ? "connected" : "on") : "off");
}

bool App::command(const char* line, char* reply, size_t len) {
  if (!strncmp(line, "look", 4) && (line[4] == 0 || line[4] == ' ')) return lookCommand(line, reply, len);
  if (!strncmp(line, "lang", 4) && (line[4] == 0 || line[4] == ' ')) {
    Lang l;
    if (!line[4]) {
      snprintf(reply, len, "lang=%s", langName(gLang));
      return true;
    }
    if (!langFromName(line + 5, l)) {
      snprintf(reply, len, "usage: lang en|nl");
      return false;
    }
    setLang(l);
    snprintf(reply, len, "ok lang=%s", langName(l));
    return true;
  }
  if (!strcmp(line, "reboot")) {
    snprintf(reply, len, "ok restarting");
    rebootIn_ = 500;  // let the reply go out first
    return true;
  }
  if (!strcmp(line, "perf")) {
    snprintf(reply, len,
             "fps=%.1f frame=%.1f ms (draw %.1f, rotate %.1f, send %.1f) loop-load=%.0f%% "
             "internal-free=%lu KB (lowest %lu KB) psram-free=%lu KB",
             dev_.fps, dev_.renderMs, dev_.renderMs - dev_.rotateMs - dev_.sendMs, dev_.rotateMs, dev_.sendMs, dev_.load,
             (unsigned long)(dev_.heapFree / 1024),
             (unsigned long)(dev_.heapMin / 1024), (unsigned long)(dev_.psramFree / 1024));
    return true;
  }
  if (!strncmp(line, "mic", 3) && (line[3] == 0 || line[3] == ' ')) {
    const char* a = line[3] ? line + 4 : "";
    if (!strcmp(a, "trace")) {
      mic.trace(reply, len);
    } else if (!strcmp(a, "on") || !strcmp(a, "off")) {
      settings_.mic = !strcmp(a, "on");
      mic.setEnabled(settings_.mic);
      if (!settings_.mic) {  // stop listening or dancing
        behaviour.onSound(SoundEvent::SpeechStop);
        behaviour.onSound(SoundEvent::MusicStop);
      }
      save();
      snprintf(reply, len, "ok mic %s", a);
    } else {
      snprintf(reply, len, "mic=%s level=%.0f dB background=%.0f dB speech=%s music=%s dancing=%s",
               settings_.mic ? "on" : "off", mic.db(), mic.floorDb(), mic.speech() ? "yes" : "no", mic.music() ? "yes" : "no",
               behaviour.dancing() ? "yes" : "no");
    }
    return true;
  }
  if (!strcmp(line, "json")) {  // status as JSON (for BLE clients; the web uses /api/status)
    statusJson(reply, len);
    return true;
  }
  return runCommand(line, face, behaviour, *this, reply, len);
}

void App::ble(const char* action, char* reply, size_t len) {
  if (!strcmp(action, "on") || !strcmp(action, "off")) {
    settings_.ble = !strcmp(action, "on");
    platform_.setBle(settings_.ble);
    save();
    snprintf(reply, len, "ok ble %s", action);
  } else if (!strcmp(action, "forget")) {
    platform_.forgetBle();
    snprintf(reply, len, "ok paired devices forgotten");
  } else {
    snprintf(reply, len, "ble=%s name=%s connected=%s", dev_.bleEnabled ? "on" : "off", dev_.bleName,
             dev_.bleConnected ? "yes" : "no");
  }
}

}  // namespace buddy
