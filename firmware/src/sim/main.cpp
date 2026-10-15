// Headless simulator: draait de echte buddy-code met LVGL op de PC en schrijft frames als PPM.
// Gebruik via tools/sim.sh (zet PPM om naar PNG/GIF).
//
//   sim <outdir> emotions   -> één snapshot per emotie + klok
//   sim <outdir> demo       -> frames van een gescript demo-scenario (30 fps)

#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string>
#include <vector>

#include "app.h"

using namespace buddy;

static constexpr int W = Face::kSize, H = Face::kSize;
static uint16_t fb[W * H];
static uint32_t simMs = 0;

static uint32_t tickCb() { return simMs; }

static void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  auto* src = (uint16_t*)px;
  int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; y++) memcpy(&fb[y * W + a->x1], &src[(y - a->y1) * w], w * 2);
  lv_display_flush_ready(d);
}

static void saveWav(const std::string& path, const std::vector<int16_t>& s) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  uint32_t rate = SoundSynth::kRate, bytes = (uint32_t)(s.size() * 2), v;
  auto w32 = [&](uint32_t x) { fwrite(&x, 4, 1, f); };
  auto w16 = [&](uint16_t x) { fwrite(&x, 2, 1, f); };
  fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
  w32(16); w16(1); w16(1); w32(rate); w32(rate * 2); w16(2); w16(16);
  fwrite("data", 1, 4, f); w32(bytes);
  fwrite(s.data(), 2, s.size(), f);
  (void)v;
  fclose(f);
}

static void savePpm(const std::string& path) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) {
    perror(path.c_str());
    exit(1);
  }
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  const float r = W / 2.0f;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      float dx = x + 0.5f - r, dy = y + 0.5f - r;
      uint8_t rgb[3];
      if (dx * dx + dy * dy > r * r) {
        rgb[0] = rgb[1] = rgb[2] = 0x30;  // buiten het ronde scherm
      } else {
        uint16_t c = fb[y * W + x];
        rgb[0] = ((c >> 11) & 0x1f) * 255 / 31;
        rgb[1] = ((c >> 5) & 0x3f) * 255 / 63;
        rgb[2] = (c & 0x1f) * 255 / 31;
      }
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
}

struct SimPlatform : Platform {
  DeviceInfo dev;
  void localTime(struct tm& now) override {
    now = {};
    now.tm_hour = 9; now.tm_min = 41; now.tm_mday = 15; now.tm_mon = 9; now.tm_wday = 3;
  }
  void deviceInfo(DeviceInfo& out) override { out = dev; }
};

struct Sim {
  SimPlatform platform;
  App app{platform};
  Face& face = app.face;
  Behaviour& behaviour = app.behaviour;

  void init() {
    lv_init();
    lv_tick_set_cb(tickCb);
    lv_display_t* d = lv_display_create(W, H);
    static uint16_t buf[W * 80];
    lv_display_set_buffers(d, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flushCb);
    platform.dev.batteryPct = 76;
    app.create(lv_screen_active());
  }

  std::vector<int16_t> audio;  // 16 kHz mono, synchroon met de frames

  void step(uint32_t ms) {
    for (uint32_t t = 0; t < ms;) {
      uint32_t dt = ms - t < 10 ? ms - t : 10;  // exact ms vooruit, zodat beeld en geluid synchroon blijven
      t += dt;
      simMs += dt;
      app.update(dt);
      lv_timer_handler();
      int16_t chunk[SoundSynth::kRate / 100];
      size_t n = SoundSynth::kRate / 1000 * dt;
      app.sound.fill(chunk, n);
      audio.insert(audio.end(), chunk, chunk + n);
    }
    lv_refr_now(nullptr);
  }
};

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "gebruik: %s <outdir> emotions|demo\n", argv[0]);
    return 2;
  }
  std::string out = argv[1];
  std::string mode = argv[2];
  Sim s;
  s.init();
  s.face.setAutoBlink(mode != "emotions");
  s.step(500);

  if (mode == "emotions") {
    for (int i = 0; i < (int)Emotion::Count; i++) {
      s.behaviour.command((Emotion)i);
      s.step(1500);
      savePpm(out + "/emotion_" + emotionName((Emotion)i) + ".ppm");
    }
    s.app.setSkin(Skin::Mascot);
    for (int i = 0; i < (int)Emotion::Count; i++) {
      s.behaviour.command((Emotion)i);
      s.step(1500);
      savePpm(out + "/mascot_" + emotionName((Emotion)i) + ".ppm");
    }
    s.face.lookAt(-0.9f, -0.6f);
    s.step(800);
    savePpm(out + "/look_mascot.ppm");
    // Acties: momentopnames op kenmerkende tijdstippen, voor beide skins
    struct Shot { Action a; uint32_t ms; const char* tag; };
    const Shot shots[] = {{Action::Whistle, 1300, "whistle"}, {Action::Wink, 350, "wink"},
                          {Action::Kiss, 1300, "kiss"},       {Action::Rage, 1500, "rage"},
                          {Action::Explode, 1250, "explode1"}, {Action::Explode, 250, "explode2"},
                          {Action::Explode, 1600, "explode3"}};
    for (Skin sk : {Skin::Classic, Skin::Mascot}) {
      s.app.setSkin(sk);
      s.behaviour.command(Emotion::Neutral);
      s.step(1200);
      for (const Shot& sh : shots) {
        // explode2/3 lopen door op de vorige (zelfde actie), de rest start opnieuw
        if (strcmp(sh.tag, "explode2") && strcmp(sh.tag, "explode3")) {
          s.step(2500);
          s.face.play(sh.a);
        }
        s.step(sh.ms);
        savePpm(out + "/action_" + skinName(sk) + "_" + sh.tag + ".ppm");
      }
      s.step(5000);
    }

    s.app.setSkin(Skin::Classic);
    s.face.releaseLook();
    s.behaviour.command(Emotion::Happy);
    s.face.lookAt(-0.9f, -0.6f);
    s.step(800);
    savePpm(out + "/look_up_left.ppm");
    s.app.showClock();
    s.step(600);
    savePpm(out + "/clock.ppm");
    s.app.showInfo();
    s.step(600);
    savePpm(out + "/info_offline.ppm");
    auto& d = s.platform.dev;
    d.wifi = DeviceInfo::WifiMode::Station;
    snprintf(d.ssid, sizeof(d.ssid), "Thuisnetwerk");
    snprintf(d.ip, sizeof(d.ip), "192.168.1.42");
    d.rssi = -58;
    d.bleEnabled = true;
    d.charging = true;
    s.step(700);
    savePpm(out + "/info_online.ppm");
    s.app.setSkin(Skin::Mascot);
    s.app.showInfo();
    s.step(700);
    savePpm(out + "/info_mascot.ppm");
    s.app.info.hide();
    d.wifi = DeviceInfo::WifiMode::AccessPoint;
    snprintf(d.ssid, sizeof(d.ssid), "deskbuddy-A45C");
    snprintf(d.ip, sizeof(d.ip), "192.168.4.1");
    s.step(600);
    s.app.showWifiSetup();
    s.step(700);
    savePpm(out + "/wifi_setup.ppm");
    d.apClients = 1;
    s.step(1200);
    savePpm(out + "/wifi_setup_step2.ppm");
    s.app.setup.hide();
    d.updateState = UpdateState::Installing;
    d.updateProgress = 42;
    snprintf(d.updateVersion, sizeof(d.updateVersion), "v0.3.0");
    s.step(900);
    savePpm(out + "/matrix_update.ppm");
    s.step(500);
    savePpm(out + "/matrix_update2.ppm");
    d.updateState = UpdateState::Idle;
    s.step(600);
    s.app.setup.hide();
    d.wifi = DeviceInfo::WifiMode::Station;
    d.apClients = 0;
    snprintf(d.ip, sizeof(d.ip), "192.168.1.42");
    d.updateState = UpdateState::Available;
    snprintf(d.updateVersion, sizeof(d.updateVersion), "v0.2.0");
    s.app.setSkin(Skin::Classic);
    s.app.showInfo();
    s.step(700);
    savePpm(out + "/update_available.ppm");
    d.updateState = UpdateState::Installing;
    d.updateProgress = 42;
    s.step(900);
    savePpm(out + "/update_installing.ppm");
    return 0;
  }

  // Demo: gescript scenario
  struct Step { uint32_t at; const char* what; };
  std::vector<Step> script = {
      {0, "neutral"},   {2500, "tap"},     {4500, "pet"},    {5200, "pet"},       {5900, "pet"},
      {9500, "shake"},  {15000, "long"},   {19500, "sad"},   {22000, "surprised"}, {23500, "sleepy"},
      {27000, "asleep"}, {29000, "mascot"}, {31000, "tap"},   {33500, "clock"}, {37500, "info"},
      {41000, "whistle"}, {45000, "wink"}, {46500, "kiss"}, {49500, "rage"}, {54000, "explode"},
      {61500, "classic"}, {62500, "kiss"}, {65500, "explode"}, {73000, "end"},
  };
  size_t next = 0;
  int frame = 0;
  size_t demoAudioStart = s.audio.size();
  for (uint32_t t = 0;; t += 33) {
    while (next < script.size() && script[next].at <= t) {
      std::string w = script[next].what;
      GestureEvent g;
      if (w == "end") {
        // Audio vanaf het eerste demo-frame (de opwarmtijd en snapshots niet meenemen).
        std::vector<int16_t> a(s.audio.begin() + demoAudioStart, s.audio.end());
        saveWav(out + "/demo.wav", a);
        return 0;
      }
      if (w == "tap") { g.type = Gesture::Tap; g.x = 0.4f; g.y = -0.2f; s.behaviour.onGesture(g); }
      else if (w == "pet") { g.type = Gesture::Pet; s.behaviour.onGesture(g); }
      else if (w == "shake") { g.type = Gesture::Shake; s.behaviour.onGesture(g); }
      else if (w == "long") { g.type = Gesture::LongPress; s.behaviour.onGesture(g); }
      else if (w == "clock") s.app.showClock();
      else if (w == "info") s.app.showInfo();
      else if (w == "mascot") s.app.setSkin(Skin::Mascot);
      else if (w == "classic") s.app.setSkin(Skin::Classic);
      else if (Action a; actionFromName(w.c_str(), a)) {
        s.app.info.hide();
        s.app.clock.hide();
        s.face.play(a);
      }
      else {
        Emotion e;
        if (emotionFromName(w.c_str(), e)) s.behaviour.command(e, 0);
      }
      next++;
    }
    s.step(33);
    char name[64];
    snprintf(name, sizeof(name), "/frame_%05d.ppm", frame++);
    savePpm(out + name);
  }
}
