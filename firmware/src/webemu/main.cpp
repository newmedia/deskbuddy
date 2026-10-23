// Web emulator: the same App as on the device, with the same web API, operated from the browser.
//
//   tools/webemu.sh        -> http://localhost:8765/emu
//
// /emu      control page: the round screen (mouse = finger), sensors, battery, Wi-Fi, BLE, sound,
//           and next to it the web interface (via "Wi-Fi") or the PWA (via a faked Bluetooth pairing)
// /         the real web interface (firmware/web/index.html, straight from disk: edit + reload)
// /api/...  same endpoints as the device (cmd, status, wifi, update)
// /emu/...  screen, input, sound and the faked BLE connection (NUS line protocol)
//
// Settings and custom profiles are kept in .pio/webemu-state/ (like NVS on the device).

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <lvgl.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "app.h"

using namespace buddy;

namespace {

constexpr int W = Face::kSize, H = Face::kSize;
uint16_t fb[W * H];
uint32_t frameNo = 1;  // increments with every new frame

std::string gRoot, gState;
char** gArgv = nullptr;

uint32_t nowMs() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000ull + ts.tv_nsec / 1000000ull);
}

void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  auto* src = (uint16_t*)px;
  int w = a->x2 - a->x1 + 1;
  for (int y = a->y1; y <= a->y2; y++) memcpy(&fb[y * W + a->x1], &src[(y - a->y1) * w], w * 2);
  lv_display_flush_ready(d);
}
void refrReadyCb(lv_event_t*) { frameNo++; }

bool readFile(const std::string& path, std::string& out) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  out.clear();
  char buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
  fclose(f);
  return true;
}
void writeFile(const std::string& path, const void* data, size_t len) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  fwrite(data, 1, len, f);
  fclose(f);
}

// ---------- Faked device ----------

struct FakeNet {
  char ssid[33] = "HomeNetwork", pass[65] = "";
  bool saved = true;
  uint32_t joinAt = 0;  // >0: joining since
  bool joinFails = false;
  const char* failReason = "";
};

struct EmuPlatform : Platform {
  DeviceInfo dev;
  FakeNet net;
  uint8_t brightness = 200;
  bool blePaired = false;
  uint32_t rebootAt = 0;
  // updates
  UpdateMode updMode = UpdateMode::Notify;
  std::string offered = "v1.0.1";  // "release" offered by the faked GitHub (empty = none)
  uint32_t checkDoneAt = 0, installStart = 0;
  bool installAfterCheck = false;

  EmuPlatform() {
    dev.wifi = DeviceInfo::WifiMode::Station;
    snprintf(dev.ssid, sizeof(dev.ssid), "%s", net.ssid);
    snprintf(dev.ip, sizeof(dev.ip), "127.0.0.1");
    dev.rssi = -58;
    snprintf(dev.version, sizeof(dev.version), "1.0.0");
    dev.batteryPct = -1;  // USB power
    dev.heapFree = 80 * 1024;
    dev.heapMin = 72 * 1024;
    dev.psramFree = 7700 * 1024;
    blePaired = access((gState + "/ble-paired").c_str(), F_OK) == 0;
  }

  // Settings and blobs: one file per key (like NVS)
  // Each setting as its own key in a text file, like NVS on the device: a new setting then simply
  // gets its default value (a binary struct dump would shift).
  void loadSettings(Settings& s) override {
    std::string d;
    if (readFile(gState + "/settings.txt", d)) {
      auto val = [&](const char* k, int def) {
        size_t i = d.find(std::string("\n") + k + "=");
        if (i == std::string::npos && d.compare(0, strlen(k) + 1, std::string(k) + "=") == 0) i = (size_t)-1;
        else if (i == std::string::npos) return def;
        return atoi(d.c_str() + i + 1 + strlen(k) + 1);
      };
      s.sound = val("sound", s.sound);
      s.volume = (uint8_t)val("volume", s.volume);
      s.brightness = (uint8_t)val("bright", s.brightness);
      s.sleepMin = (uint8_t)val("sleep", s.sleepMin);
      s.ble = val("ble", s.ble);
      s.mic = val("mic", s.mic);
      s.lang = (uint8_t)val("lang", s.lang);
    }
    std::string l;
    if (readFile(gState + "/look.bin", l) && l.size() == sizeof(Look)) memcpy(&s.look, l.data(), sizeof(Look));
    dev.bleEnabled = s.ble;
  }
  void saveSettings(const Settings& s) override {
    char t[160];
    int n = snprintf(t, sizeof(t), "\nsound=%d\nvolume=%u\nbright=%u\nsleep=%u\nble=%d\nmic=%d\nlang=%u\n", s.sound, s.volume,
                     s.brightness, s.sleepMin, s.ble, s.mic, s.lang);
    writeFile(gState + "/settings.txt", t, (size_t)n);
    writeFile(gState + "/look.bin", &s.look, sizeof(Look));
  }
  size_t loadBlob(const char* key, void* buf, size_t max) override {
    std::string d;
    if (!readFile(gState + "/" + key + ".bin", d)) return 0;
    size_t n = LV_MIN(d.size(), max);
    memcpy(buf, d.data(), n);
    return n;
  }
  void saveBlob(const char* key, const void* buf, size_t len) override {
    writeFile(gState + "/" + key + ".bin", buf, len);
  }
  void localTime(struct tm& out) override {
    time_t t = time(nullptr);
    localtime_r(&t, &out);
  }
  void deviceInfo(DeviceInfo& out) override { out = dev; }
  void setBrightness(uint8_t v) override { brightness = v; }
  void reboot() override { rebootAt = nowMs() + 300; }
  void startWifiSetup() override {
    if (dev.wifi == DeviceInfo::WifiMode::Station) return;
    setAp();
  }
  void setBle(bool on) override {
    dev.bleEnabled = on;
    if (!on) dev.bleConnected = false;
  }
  void forgetBle() override {
    blePaired = false;
    dev.bleConnected = false;
    unlink((gState + "/ble-paired").c_str());
  }

  void setAp() {
    dev.wifi = DeviceInfo::WifiMode::AccessPoint;
    snprintf(dev.ssid, sizeof(dev.ssid), "deskbuddy-A45C");
    snprintf(dev.ip, sizeof(dev.ip), "192.168.4.1");
    dev.rssi = 0;
  }
  void setStation() {
    dev.wifi = DeviceInfo::WifiMode::Station;
    snprintf(dev.ssid, sizeof(dev.ssid), "%s", net.ssid);
    snprintf(dev.ip, sizeof(dev.ip), "127.0.0.1");
    dev.rssi = -58;
  }
  void join(const char* s, const char* p) {
    snprintf(net.ssid, sizeof(net.ssid), "%s", s);
    snprintf(net.pass, sizeof(net.pass), "%s", p ? p : "");
    net.saved = true;
    net.joinAt = nowMs();
    // Test cases: password "wrong" = wrong password, network "Gone" = unreachable
    net.joinFails = !strcmp(net.pass, "wrong") || !strcmp(net.ssid, "Gone");
    net.failReason = !strcmp(net.ssid, "Gone") ? "network not found" : "wrong password";
    dev.wifi = DeviceInfo::WifiMode::Connecting;
    snprintf(dev.ssid, sizeof(dev.ssid), "%s", s);
    dev.ip[0] = 0;
  }
  const char* scanJson() {
    return "[{\"ssid\":\"HomeNetwork\",\"rssi\":-52,\"open\":false},"
           "{\"ssid\":\"Neighbours 5G\",\"rssi\":-71,\"open\":false},"
           "{\"ssid\":\"GuestNetwork\",\"rssi\":-80,\"open\":true},"
           "{\"ssid\":\"Gone\",\"rssi\":-90,\"open\":false}]";
  }
  void wifi(const char* act, const char* s, const char* p, char* reply, size_t len) override {
    if (!strcmp(act, "off")) {
      dev.wifi = DeviceInfo::WifiMode::Off;
      dev.ssid[0] = dev.ip[0] = 0;
      snprintf(reply, len, "ok wifi off");
    } else if (!strcmp(act, "on")) {
      if (net.saved) join(net.ssid, net.pass);
      else setAp();
      snprintf(reply, len, "ok wifi on");
    } else if (!strcmp(act, "join")) {
      join(s, p);
      snprintf(reply, len, "ok connecting to %s", s);
    } else if (!strcmp(act, "scan")) {
      snprintf(reply, len, "{\"networks\":%s}", scanJson());
    } else if (!strcmp(act, "forget")) {
      net.saved = false;
      net.ssid[0] = net.pass[0] = 0;
      dev.wifi = DeviceInfo::WifiMode::Off;
      dev.ssid[0] = dev.ip[0] = 0;
      snprintf(reply, len, "ok network forgotten");
    } else {
      snprintf(reply, len, "mode=%d ssid=%s ip=%s rssi=%d heap=%u", (int)dev.wifi, dev.ssid, dev.ip, dev.rssi,
               (unsigned)dev.heapFree);
    }
  }

  void firmwareUpdate(const char* act, char* reply, size_t len) override {
    UpdateMode m;
    if (updateModeFromName(act, m)) {
      updMode = m;
      dev.updateMode = m;
      snprintf(reply, len, "ok updates=%s", updateModeName(m));
      if (m == UpdateMode::Auto && dev.updateState == UpdateState::Available) startInstall();
    } else if (!strcmp(act, "check")) {
      startCheck(false);
      snprintf(reply, len, "ok checking...");
    } else if (!strcmp(act, "install")) {
      if (dev.updateState == UpdateState::Installing) snprintf(reply, len, "installing in progress");
      else if (dev.updateState == UpdateState::Available) {
        startInstall();
        snprintf(reply, len, "ok installing %s...", dev.updateVersion);
      } else {
        startCheck(true);
        snprintf(reply, len, "ok checking and installing...");
      }
    } else {
      snprintf(reply, len, "updates=%s status=%s current=%s latest=%s internal-free=%u", updateModeName(updMode),
               updateStateName(dev.updateState), dev.version, dev.updateVersion[0] ? dev.updateVersion : "-",
               (unsigned)dev.heapFree);
    }
  }
  void startCheck(bool thenInstall) {
    dev.updateState = UpdateState::Checking;
    checkDoneAt = nowMs() + 1200;
    installAfterCheck = thenInstall;
  }
  void startInstall() {
    dev.updateState = UpdateState::Installing;
    dev.updateProgress = 0;
    installStart = nowMs();
  }

  void tick(uint32_t now) {
    if (net.joinAt && now - net.joinAt > 1800) {
      net.joinAt = 0;
      if (net.joinFails) dev.wifi = DeviceInfo::WifiMode::Off;
      else setStation();
    }
    if (checkDoneAt && (int32_t)(now - checkDoneAt) >= 0) {
      checkDoneAt = 0;
      if (offered.empty()) {
        dev.updateState = UpdateState::UpToDate;
        dev.updateVersion[0] = 0;
      } else {
        dev.updateState = UpdateState::Available;
        snprintf(dev.updateVersion, sizeof(dev.updateVersion), "%s", offered.c_str());
        if (installAfterCheck || updMode == UpdateMode::Auto) startInstall();
      }
    }
    if (dev.updateState == UpdateState::Installing) {
      uint32_t t = now - installStart;
      dev.updateProgress = (uint8_t)LV_MIN(100u, t / 60);  // ~6 s
      if (t > 6500) {
        if (dev.updateVersion[0]) snprintf(dev.version, sizeof(dev.version), "%s", dev.updateVersion + 1);
        writeFile(gState + "/version", dev.version, strlen(dev.version));
        rebootAt = now;
      }
    }
  }
};

// ---------- Minimal HTTP server (one request per connection) ----------

struct Request {
  std::string method, path, query, body, contentType;
};

std::string urlDecode(const std::string& s) {
  std::string o;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') o += ' ';
    else if (s[i] == '%' && i + 2 < s.size()) {
      o += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
      i += 2;
    } else o += s[i];
  }
  return o;
}
std::string formArg(const std::string& form, const char* key) {
  size_t i = 0;
  std::string k = std::string(key) + "=";
  while (i <= form.size()) {
    size_t e = form.find('&', i);
    if (e == std::string::npos) e = form.size();
    if (form.compare(i, k.size(), k) == 0) return urlDecode(form.substr(i + k.size(), e - i - k.size()));
    i = e + 1;
  }
  return "";
}
bool hasArg(const Request& r, const char* key) {
  // "key=..." or a bare "key" (like ?pwa)
  auto has = [&](const std::string& f) {
    size_t i = 0, n = strlen(key);
    while (i <= f.size()) {
      size_t e = f.find('&', i);
      if (e == std::string::npos) e = f.size();
      if (f.compare(i, n, key) == 0 && (i + n == e || f[i + n] == '=')) return true;
      i = e + 1;
    }
    return false;
  };
  return has(r.query) || (r.contentType.find("urlencoded") != std::string::npos && has(r.body));
}
std::string arg(const Request& r, const char* key) {
  std::string v = formArg(r.query, key);
  if (v.empty() && r.contentType.find("urlencoded") != std::string::npos) v = formArg(r.body, key);
  return v;
}

bool readRequest(int fd, Request& r) {
  std::string data;
  char buf[65536];
  size_t headEnd = std::string::npos;
  size_t want = 0;
  for (;;) {
    pollfd p = {fd, POLLIN, 0};
    if (poll(&p, 1, 3000) <= 0) return false;
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) return false;
    data.append(buf, (size_t)n);
    if (headEnd == std::string::npos) {
      headEnd = data.find("\r\n\r\n");
      if (headEnd == std::string::npos) continue;
      std::string head = data.substr(0, headEnd);
      size_t sp1 = head.find(' '), sp2 = head.find(' ', sp1 + 1);
      r.method = head.substr(0, sp1);
      std::string target = head.substr(sp1 + 1, sp2 - sp1 - 1);
      size_t q = target.find('?');
      r.path = target.substr(0, q);
      r.query = q == std::string::npos ? "" : target.substr(q + 1);
      size_t cl = 0;
      size_t pos = 0;
      while ((pos = head.find("\r\n", pos)) != std::string::npos) {
        pos += 2;
        size_t e = head.find("\r\n", pos);
        std::string line = head.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        size_t c = line.find(':');
        if (c == std::string::npos) continue;
        std::string k = line.substr(0, c), v = line.substr(c + 1);
        while (!v.empty() && v[0] == ' ') v.erase(0, 1);
        for (auto& ch : k) ch = (char)tolower(ch);
        if (k == "content-length") cl = (size_t)atol(v.c_str());
        if (k == "content-type") r.contentType = v;
      }
      want = headEnd + 4 + cl;
    }
    if (data.size() >= want) {
      r.body = data.substr(headEnd + 4);
      return true;
    }
  }
}

void sendAll(int fd, const char* p, size_t n) {
  while (n) {
    ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
    if (w <= 0) return;
    p += w;
    n -= (size_t)w;
  }
}
void respond(int fd, int code, const char* type, const std::string& body, const char* extra = "") {
  const char* status = code == 200 ? "OK" : code == 204 ? "No Content" : code == 404 ? "Not Found" : "Bad Request";
  char head[512];
  int n = snprintf(head, sizeof(head),
                   "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
                   "Connection: close\r\n%s\r\n",
                   code, status, type, body.size(), extra);
  sendAll(fd, head, (size_t)n);
  sendAll(fd, body.data(), body.size());
}

std::string jsonEscape(const char* s) {
  std::string o;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { o += '\\'; o += *s; }
    else if (*s == '\n') o += "\\n";
    else if ((unsigned char)*s < 0x20) o += ' ';
    else o += *s;
  }
  return o;
}

// ---------- Faked microphone: background noise plus test sounds (clap, music, ...) ----------

struct MicScene {
  uint32_t seed = 12345;
  uint64_t t = 0;  // samples
  uint64_t until = 0;
  enum Kind { Quiet, Clap, Double, Loud, Music, Speech } kind = Quiet;
  uint64_t start = 0;
  uint64_t nextToggle = 0;  // speech: syllable on/off
  bool voiced = false;
  float room = 0;
  float rnd() {
    seed = seed * 1664525u + 1013904223u;
    return (seed >> 8) / 16777216.0f * 2 - 1;  // -1..1
  }
  void play(const std::string& v) {
    start = t;
    kind = v == "clap" ? Clap : v == "double" ? Double : v == "loud" ? Loud : v == "music" ? Music
         : v == "speech" ? Speech : Quiet;
    const uint64_t s = MicListener::kRate;
    until = t + (kind == Clap ? s / 2 : kind == Double ? s : kind == Loud ? 3 * s : kind == Music ? 12 * s
                 : kind == Speech ? 8 * s : 0);
    nextToggle = t;
  }
  const char* name() const {
    static const char* const n[] = {"quiet", "clap", "double clap", "noise", "music", "speech"};
    return t < until ? n[kind] : n[0];
  }
  int16_t next() {
    const uint64_t s = MicListener::kRate;
    // Room noise that (like the real thing) fluctuates: ~ -65 to -50 dBFS, slowly up and down
    room += (rnd() * 0.002f);
    room = LV_CLAMP(-1.f, room, 1.f);
    float v = rnd() * 60 * (1.6f + room + 0.6f * sinf((float)t / MicListener::kRate * 2.3f));
    if (t < until) {
      uint64_t dt = t - start;
      auto burst = [&](uint64_t at, uint64_t len, float amp) {
        if (dt >= at && dt < at + len) v += rnd() * amp * (1.0f - (float)(dt - at) / len);
      };
      switch (kind) {
        case Clap: burst(0, s / 40, 18000); break;  // 25 ms
        case Double: burst(0, s / 40, 18000); burst(s * 3 / 10, s / 40, 18000); break;
        case Loud: v += rnd() * 9000; break;
        case Music: {
          uint64_t beat = s / 2;  // 120 bpm
          burst((dt / beat) * beat, s / 16, 14000);                            // kick
          v += 2500 * sinf(2 * 3.14159f * 110 * (float)dt / s) + rnd() * 600;  // bass + noise, continuous
          break;
        }
        case Speech:
          if (t >= nextToggle) {
            voiced = !voiced;
            // syllables 80-250 ms, pauses 40-300 ms, now and then a breath pause of ~0.8 s
            float r = (rnd() + 1) / 2;
            nextToggle = t + (uint64_t)(voiced ? (0.08f + 0.17f * r) * s : (r > 0.85f ? 0.8f : 0.04f + 0.26f * r) * s);
          }
          if (voiced) v += rnd() * 2600;
          break;
        default: break;
      }
    }
    t++;
    return (int16_t)LV_CLAMP(-32767.f, v, 32767.f);
  }
};
MicScene micScene;

// ---------- Sound: what the synth produces goes into a ring buffer the browser fetches ----------

constexpr size_t kAudioRing = SoundSynth::kRate * 4;
int16_t audioRing[kAudioRing];
uint64_t audioTotal = 0;  // number of samples ever produced

}  // namespace

int main(int argc, char** argv) {
  gArgv = argv;
  prctl(PR_SET_NAME, "program");  // otherwise "exe" after a restart (execv of /proc/self/exe): keeps pkill -x program working
  gRoot = argc > 1 ? argv[1] : ".";
  int port = argc > 2 ? atoi(argv[2]) : 8765;
  // WEBEMU_STATE: other directory for the saved state (the regression test uses its own)
  const char* st = getenv("WEBEMU_STATE");
  gState = st && *st ? st : gRoot + "/firmware/.pio/webemu-state";
  mkdir((gRoot + "/firmware/.pio").c_str(), 0755);
  mkdir(gState.c_str(), 0755);
  signal(SIGPIPE, SIG_IGN);

  lv_init();
  lv_tick_set_cb(nowMs);
  lv_display_t* disp = lv_display_create(W, H);
  static uint16_t buf[W * H];  // full-screen, like on the device
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_add_event_cb(disp, refrReadyCb, LV_EVENT_REFR_READY, nullptr);

  EmuPlatform platform;
  {
    std::string v;
    if (readFile(gState + "/version", v) && !v.empty() && v.size() < sizeof(platform.dev.version))
      snprintf(platform.dev.version, sizeof(platform.dev.version), "%s", v.c_str());
  }
  App app(platform);
  app.create(lv_screen_active());
  app.showSplash();

  int srv = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // this computer only
  if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(srv, 16) != 0) {
    fprintf(stderr, "cannot open port %d: %s\n", port, strerror(errno));
    return 1;
  }
  printf("deskbuddy web emulator: http://localhost:%d/emu\n", port);
  fflush(stdout);

  bool touchDown = false;
  int touchX = 0, touchY = 0;
  float tiltX = 0, tiltY = 0;
  uint32_t last = nowMs(), lastTouch = 0, lastImu = 0, statWindow = last, busyUs = 0, framesAtWindow = frameNo;
  uint32_t scanReadyAt = 0;
  uint32_t pairCode = 0;
  static char reply[2048];

  for (;;) {
    pollfd p = {srv, POLLIN, 0};
    if (poll(&p, 1, 5) > 0) {
      int fd = accept(srv, nullptr, nullptr);
      if (fd >= 0) {
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        Request r;
        if (readRequest(fd, r)) {
          const std::string& P = r.path;
          std::string html;
          if (P == "/" || P == "/index.html") {
            readFile(gRoot + "/firmware/web/index.html", html);
            // PWA mode: fake Bluetooth (same NUS line protocol as on the device)
            if (hasArg(r, "pwa")) {
              size_t i = html.find("<script>");
              if (i != std::string::npos) html.insert(i, "<script src=\"/emu/fake-bluetooth.js\"></script>\n");
            }
            respond(fd, 200, "text/html; charset=utf-8", html);
          } else if (P == "/manifest.webmanifest" || P == "/sw.js" || P == "/icon.svg") {
            std::string f;
            readFile(gRoot + "/firmware/web" + P, f);
            respond(fd, 200, P == "/icon.svg" ? "image/svg+xml" : P == "/sw.js" ? "text/javascript" : "application/json", f);
          } else if (P == "/emu" || P == "/emu/") {
            readFile(gRoot + "/firmware/src/webemu/emu.html", html);
            respond(fd, 200, "text/html; charset=utf-8", html);
          } else if (P == "/emu/fake-bluetooth.js") {
            std::string f;
            readFile(gRoot + "/firmware/src/webemu/fake-bluetooth.js", f);
            respond(fd, 200, "text/javascript", f);
          } else if (P == "/api/status") {
            app.statusJson(reply, sizeof(reply));
            respond(fd, 200, "application/json", reply);
          } else if (P == "/api/cmd") {
            std::string line = hasArg(r, "c") ? arg(r, "c") : r.body;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) line.pop_back();
            bool ok = app.command(line.c_str(), reply, sizeof(reply));
            respond(fd, ok ? 200 : 400, "application/json",
                    std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"reply\":\"" + jsonEscape(reply) + "\"}");
          } else if (P == "/api/wifi/scan") {
            uint32_t now = nowMs();
            if (!scanReadyAt) scanReadyAt = now + 1500;
            if ((int32_t)(now - scanReadyAt) < 0) respond(fd, 200, "application/json", "{\"scanning\":true}");
            else {
              scanReadyAt = 0;
              respond(fd, 200, "application/json", std::string("{\"scanning\":false,\"networks\":") + platform.scanJson() + "}");
            }
          } else if (P == "/api/wifi" && r.method == "POST") {
            std::string s = arg(r, "ssid"), pw = arg(r, "pass");
            if (s.empty() || s.size() > 32 || pw.size() > 64) {
              respond(fd, 400, "application/json", "{\"ok\":false,\"reply\":\"invalid ssid or password\"}");
            } else {
              respond(fd, 200, "application/json", "{\"ok\":true,\"reply\":\"connecting...\"}");
              platform.join(s.c_str(), pw.c_str());
            }
          } else if (P == "/api/wifi/state") {
            const DeviceInfo& d = platform.dev;
            const char* stt = d.wifi == DeviceInfo::WifiMode::Station ? "connected"
                              : platform.net.joinAt                     ? "connecting"
                              : platform.net.joinFails                  ? "failed"
                                                                        : "idle";
            bool failed = !strcmp(stt, "failed");
            snprintf(reply, sizeof(reply), "{\"sta\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"reason\":\"%s\",\"switchIn\":0}",
                     stt, jsonEscape(platform.net.ssid).c_str(), d.wifi == DeviceInfo::WifiMode::Station ? d.ip : "",
                     failed ? platform.net.failReason : "");
            respond(fd, 200, "application/json", reply);
          } else if (P == "/update" && r.method == "POST") {
            // Upload via the web interface: check the size, then "install" (screen) and restart
            bool ok = r.body.size() > 100 * 1024;
            if (ok) {
              snprintf(platform.dev.updateVersion, sizeof(platform.dev.updateVersion), "%s", "");
              platform.startInstall();
            }
            respond(fd, ok ? 200 : 500, "application/json",
                    ok ? "{\"ok\":true,\"reply\":\"updated, restarting...\"}" : "{\"ok\":false,\"reply\":\"update failed\"}");
          } else if (P == "/emu/screen") {
            uint32_t since = (uint32_t)atol(arg(r, "f").c_str());
            char extra[96];
            snprintf(extra, sizeof(extra), "X-Frame: %u\r\nX-Bright: %u\r\n", frameNo, platform.brightness);
            if (since == frameNo) respond(fd, 204, "application/octet-stream", "", extra);
            else respond(fd, 200, "application/octet-stream", std::string((const char*)fb, sizeof(fb)), extra);
          } else if (P == "/emu/audio") {
            // PCM16 mono since 'pos' (at most ~1 s back)
            uint64_t pos = (uint64_t)atoll(arg(r, "pos").c_str());
            if (pos == 0 || audioTotal - pos > SoundSynth::kRate) pos = audioTotal;
            std::string out;
            for (uint64_t i = pos; i < audioTotal; i++) {
              int16_t s = audioRing[i % kAudioRing];
              out.append((const char*)&s, 2);
            }
            char extra[64];
            snprintf(extra, sizeof(extra), "X-Pos: %llu\r\n", (unsigned long long)audioTotal);
            respond(fd, 200, "application/octet-stream", out, extra);
          } else if (P == "/emu/state") {
            const DeviceInfo& d = platform.dev;
            snprintf(reply, sizeof(reply),
                     "{\"wifi\":%d,\"battery\":%d,\"charging\":%s,\"ble\":%s,\"bleConnected\":%s,\"blePaired\":%s,"
                     "\"pairCode\":%u,\"offered\":\"%s\",\"version\":\"%s\",\"emotion\":\"%s\",\"brightness\":%u,"
                     "\"sound\":%s,\"view\":\"%s\",\"micScene\":\"%s\",\"dancing\":%s,\"listening\":%s,\"emotions\":[",
                     (int)d.wifi, d.batteryPct, d.charging ? "true" : "false", d.bleEnabled ? "true" : "false",
                     d.bleConnected ? "true" : "false", platform.blePaired ? "true" : "false", pairCode,
                     platform.offered.c_str(), d.version, emotionName(app.face.emotion()), platform.brightness,
                     app.sound.enabled() ? "true" : "false",
                     app.setup.visible() ? "setup" : app.info.visible() ? "info" : app.clock.visible() ? "clock" : "face",
                     micScene.name(), app.behaviour.dancing() ? "true" : "false", app.face.listening() ? "true" : "false");
            // All emotions and actions, so the regression test (tools/webemu-test.sh) picks up new ones automatically
            std::string js = reply;
            for (int i = 0; i < (int)Emotion::Count; i++)
              js += std::string(i ? "," : "") + "\"" + emotionName((Emotion)i) + "\"";
            js += "],\"actions\":[";
            for (int i = 1; i < (int)Action::Count; i++)
              js += std::string(i > 1 ? "," : "") + "\"" + actionName((Action)i) + "\"";
            js += "]}";
            respond(fd, 200, "application/json", js);
          } else if (P == "/emu/input") {
            std::string t = arg(r, "t");
            if (t == "touch") {
              touchDown = arg(r, "down") == "1";
              touchX = atoi(arg(r, "x").c_str());
              touchY = atoi(arg(r, "y").c_str());
            } else if (t == "tilt") {
              tiltX = (float)atof(arg(r, "x").c_str());
              tiltY = (float)atof(arg(r, "y").c_str());
            } else if (t == "gesture") {
              GestureEvent g;
              std::string n = arg(r, "g");
              g.type = n == "shake" ? Gesture::Shake : n == "pickup" ? Gesture::PickUp : Gesture::None;
              if (g.type != Gesture::None) app.onGesture(g);
            } else if (t == "battery") {
              platform.dev.batteryPct = atoi(arg(r, "v").c_str());
            } else if (t == "charging") {
              platform.dev.charging = arg(r, "v") == "1";
            } else if (t == "wifi") {
              std::string m = arg(r, "v");
              if (m == "off") platform.wifi("off", "", "", reply, sizeof(reply));
              else if (m == "ap") platform.setAp();
              else if (m == "sta") platform.setStation();
              else if (m == "rssi") platform.dev.rssi = atoi(arg(r, "n").c_str());
            } else if (t == "sound") {
              micScene.play(arg(r, "v"));
            } else if (t == "offer") {
              platform.offered = arg(r, "v");
            }
            respond(fd, 200, "application/json", "{\"ok\":true}");
          } else if (P == "/emu/ble") {
            // Faked NUS connection. a=connect|pair|disconnect|line
            std::string a = arg(r, "a");
            DeviceInfo& d = platform.dev;
            if (a == "connect") {
              if (!d.bleEnabled) respond(fd, 400, "text/plain", "Bluetooth is off on the deskbuddy");
              else if (platform.blePaired) {
                d.bleConnected = true;
                respond(fd, 200, "text/plain", "bonded");
              } else {
                pairCode = 100000 + (uint32_t)(rand() % 900000);
                d.blePasskey = pairCode;  // appears on the screen, like with real pairing
                respond(fd, 200, "text/plain", "pair");
              }
            } else if (a == "pair") {
              bool ok = pairCode && (uint32_t)atol(arg(r, "code").c_str()) == pairCode;
              d.blePasskey = 0;
              pairCode = 0;
              if (ok) {
                platform.blePaired = true;
                writeFile(gState + "/ble-paired", "1", 1);
                d.bleConnected = true;
              }
              respond(fd, ok ? 200 : 400, "text/plain", ok ? "ok" : "pairing failed (wrong code)");
            } else if (a == "disconnect") {
              d.bleConnected = false;
              d.blePasskey = 0;
              pairCode = 0;
              respond(fd, 200, "text/plain", "ok");
            } else {
              std::string line = r.body;
              while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
              if (!d.bleConnected) respond(fd, 400, "text/plain", "not connected");
              else {
                app.command(line.c_str(), reply, 1200);  // same maximum reply length as ble.cpp
                respond(fd, 200, "text/plain", std::string(reply) + "\n");
              }
            }
          } else {
            respond(fd, 404, "text/plain", "not found");
          }
        }
        close(fd);
      }
    }

    uint32_t t0 = nowMs();
    if (t0 - lastTouch >= 15) {
      lastTouch = t0;
      app.onTouch(touchDown, touchX, touchY, t0);
    }
    if (t0 - lastImu >= 20) {
      lastImu = t0;
      float az = sqrtf(LV_MAX(0.0f, 1 - tiltX * tiltX - tiltY * tiltY));
      app.onAccel(tiltX, tiltY, az, t0);
    }
    platform.tick(t0);
    uint32_t dt = t0 - last;
    last = t0;
    app.update(dt);
    lv_timer_handler();
    // Produce sound at the same pace as real time
    size_t n = (size_t)(SoundSynth::kRate / 1000 * dt);
    while (n) {
      int16_t chunk[512];
      size_t k = LV_MIN(n, sizeof(chunk) / 2);
      if (app.sound.fill(chunk, k)) app.mic.hold(300);  // don't hear its own sound, like on the device
      for (size_t i = 0; i < k; i++) audioRing[(audioTotal + i) % kAudioRing] = chunk[i];
      int16_t in[512];
      for (size_t i = 0; i < k; i++) in[i] = micScene.next();
      app.mic.feed(in, k);
      audioTotal += k;
      n -= k;
    }
    busyUs += (nowMs() - t0) * 1000;
    if (t0 - statWindow >= 1000) {
      float win = (t0 - statWindow) / 1000.0f;
      platform.dev.fps = (frameNo - framesAtWindow) / win;
      platform.dev.load = LV_MIN(100.0f, busyUs / 10.0f / (t0 - statWindow));
      framesAtWindow = frameNo;
      busyUs = 0;
      statWindow = t0;
    }
    if (platform.rebootAt && (int32_t)(t0 - platform.rebootAt) >= 0) {
      printf("restarting...\n");
      fflush(stdout);
      close(srv);
      execv("/proc/self/exe", gArgv);  // like ESP.restart(): start over with the saved settings
      return 0;
    }
  }
}
