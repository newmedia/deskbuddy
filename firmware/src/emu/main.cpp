// Interactive emulator (SDL2): the same App as on the device, in a window on the PC.
//
// Mouse = finger: click = tap, hold = long press, drag back and forth = pet, swipe = swipe.
// Keys: 1-9 emotions · S shake · P pick up · arrows tilt · B blink
//          C clock · I info · W toggle Wi-Fi status · L BLE on/off · M sound on/off · K skin
//          F whistle · N wink · J kiss · R rage · E explode · H sneeze · G laugh · O roll eyes
//          U hiccup · +/- battery · Esc quit

#include <SDL.h>
#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app.h"

using namespace buddy;

namespace {

constexpr int W = Face::kSize, H = Face::kSize;
uint16_t fb[W * H];
bool dirty = true;

uint32_t tickCb() { return SDL_GetTicks(); }

void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  auto* src = (uint16_t*)px;
  int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; y++) memcpy(&fb[y * W + a->x1], &src[(y - a->y1) * w], w * 2);
  dirty = true;
  lv_display_flush_ready(d);
}

struct EmuPlatform : Platform {
  DeviceInfo dev;
  uint8_t brightness = 200;
  void localTime(struct tm& out) override {
    time_t t = time(nullptr);
    localtime_r(&t, &out);
  }
  void deviceInfo(DeviceInfo& out) override { out = dev; }
  void setBrightness(uint8_t v) override {
    brightness = v;
    dirty = true;
  }
};

void cycleWifi(DeviceInfo& d) {
  using M = DeviceInfo::WifiMode;
  switch (d.wifi) {
    case M::Off:
      d.wifi = M::Connecting;
      snprintf(d.ssid, sizeof(d.ssid), "HomeNetwork");
      d.ip[0] = 0;
      break;
    case M::Connecting:
      d.wifi = M::Station;
      snprintf(d.ip, sizeof(d.ip), "192.168.1.42");
      d.rssi = -58;
      break;
    case M::Station:
      d.wifi = M::AccessPoint;
      snprintf(d.ssid, sizeof(d.ssid), "deskbuddy-A45C");
      snprintf(d.ip, sizeof(d.ip), "192.168.4.1");
      break;
    default:
      d.wifi = M::Off;
      d.ssid[0] = d.ip[0] = 0;
      break;
  }
}

}  // namespace

int main(int, char**) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* win = SDL_CreateWindow("deskbuddy emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  SDL_RenderSetLogicalSize(ren, W, H);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, W, H);

 // Round screen: make everything outside the circle grey via an overlay texture.
  SDL_Texture* mask = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, W, H);
  {
    static uint32_t px[W * H];
    const float r = W / 2.0f;
    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++) {
        float dx = x + 0.5f - r, dy = y + 0.5f - r;
        float d = sqrtf(dx * dx + dy * dy) - r;  // >0 = outside
        uint8_t a = d <= -1 ? 0 : (d >= 0 ? 255 : (uint8_t)((d + 1) * 255));
        px[y * W + x] = 0x30303000u | a;
      }
    SDL_UpdateTexture(mask, nullptr, px, W * 4);
    SDL_SetTextureBlendMode(mask, SDL_BLENDMODE_BLEND);
  }

  lv_init();
  lv_tick_set_cb(tickCb);
  lv_display_t* disp = lv_display_create(W, H);
  static uint16_t buf[W * 80];
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);

  EmuPlatform platform;
  platform.dev.batteryPct = 76;
  App app(platform);
  app.create(lv_screen_active());

  SDL_AudioSpec want = {}, have = {};
  want.freq = SoundSynth::kRate;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 512;
  want.userdata = &app.sound;
  want.callback = [](void* user, Uint8* stream, int len) {
    static_cast<SoundSynth*>(user)->fill((int16_t*)stream, (size_t)len / 2);
  };
  SDL_AudioDeviceID audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (audio) SDL_PauseAudioDevice(audio, 0);
  else fprintf(stderr, "no audio: %s\n", SDL_GetError());

  bool mouseDown = false;
  int mx = 0, my = 0;
  float tiltX = 0, tiltY = 0;
  uint32_t last = SDL_GetTicks(), lastTouch = 0, lastImu = 0;
  Emotion shown = Emotion::Count;
  bool running = true;

  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          running = false;
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (e.button.button == SDL_BUTTON_LEFT) mouseDown = true;
          mx = e.button.x;
          my = e.button.y;
          break;
        case SDL_MOUSEBUTTONUP:
          if (e.button.button == SDL_BUTTON_LEFT) mouseDown = false;
          break;
        case SDL_MOUSEMOTION:
          mx = e.motion.x;
          my = e.motion.y;
          break;
        case SDL_KEYDOWN: {
          SDL_Keycode k = e.key.keysym.sym;
          GestureEvent g;
          if (k == SDLK_ESCAPE) running = false;
          else if (k >= SDLK_1 && k <= SDLK_9) app.behaviour.command((Emotion)(k - SDLK_1), 4000);
          else if (k == SDLK_s) { g.type = Gesture::Shake; app.onGesture(g); }
          else if (k == SDLK_p) { g.type = Gesture::PickUp; app.onGesture(g); }
          else if (k == SDLK_b) app.face.blink();
          else if (k == SDLK_c) app.showClock();
          else if (k == SDLK_i) app.showInfo();
          else if (k == SDLK_w) cycleWifi(platform.dev);
          else if (k == SDLK_l) platform.dev.bleEnabled = !platform.dev.bleEnabled;
          else if (k == SDLK_m) app.setSound(!app.sound.enabled());
          else if (k == SDLK_f) app.face.play(Action::Whistle);
          else if (k == SDLK_n) app.face.play(Action::Wink);
          else if (k == SDLK_j) app.face.play(Action::Kiss);
          else if (k == SDLK_r) app.face.play(Action::Rage);
          else if (k == SDLK_e) app.face.play(Action::Explode);
          else if (k == SDLK_h) app.face.play(Action::Sneeze);
          else if (k == SDLK_g) app.face.play(Action::Laugh);
          else if (k == SDLK_o) app.face.play(Action::EyeRoll);
          else if (k == SDLK_u) app.face.play(Action::Hiccup);
          else if (k == SDLK_k) app.setSkin(app.face.skin() == Skin::Mascot ? Skin::Classic : Skin::Mascot);
          else if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS)
            platform.dev.batteryPct = platform.dev.batteryPct < 0 ? 5 : LV_MIN(100, platform.dev.batteryPct + 5);
          else if (k == SDLK_MINUS || k == SDLK_KP_MINUS)
            platform.dev.batteryPct = platform.dev.batteryPct <= 0 ? -1 : platform.dev.batteryPct - 5;
          else if (k == SDLK_LEFT) tiltX = -0.5f;
          else if (k == SDLK_RIGHT) tiltX = 0.5f;
          else if (k == SDLK_UP) tiltY = -0.5f;
          else if (k == SDLK_DOWN) tiltY = 0.5f;
          break;
        }
        case SDL_KEYUP: {
          SDL_Keycode k = e.key.keysym.sym;
          if (k == SDLK_LEFT || k == SDLK_RIGHT) tiltX = 0;
          if (k == SDLK_UP || k == SDLK_DOWN) tiltY = 0;
          break;
        }
        default:
          break;
      }
    }

    uint32_t now = SDL_GetTicks();
    if (now - lastTouch >= 15) {
      lastTouch = now;
      app.onTouch(mouseDown, mx, my, now);
    }
    if (now - lastImu >= 20) {
      lastImu = now;
      float az = sqrtf(LV_MAX(0.0f, 1 - tiltX * tiltX - tiltY * tiltY));
      app.onAccel(tiltX, tiltY, az, now);
    }
    app.update(now - last);
    last = now;
    lv_timer_handler();

    if (app.face.emotion() != shown) {
      shown = app.face.emotion();
      char title[64];
      snprintf(title, sizeof(title), "deskbuddy emulator  —  %s", emotionName(shown));
      SDL_SetWindowTitle(win, title);
    }
    if (dirty) {
      dirty = false;
      SDL_UpdateTexture(tex, nullptr, fb, W * 2);
      // 200 = normal brightness; sleeping (40) becomes visibly darker
      uint8_t b = (uint8_t)LV_CLAMP(40, platform.brightness * 255 / 200, 255);
      SDL_SetTextureColorMod(tex, b, b, b);
      SDL_RenderClear(ren);
      SDL_RenderCopy(ren, tex, nullptr, nullptr);
      SDL_RenderCopy(ren, mask, nullptr, nullptr);
      SDL_RenderPresent(ren);
    } else {
      SDL_Delay(4);
    }
  }
  SDL_Quit();
  return 0;
}
