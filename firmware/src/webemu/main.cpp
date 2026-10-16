// Web-emulator: dezelfde App als op het apparaat, met dezelfde web-API, te bedienen vanuit de browser.
//
//   tools/webemu.sh        -> http://localhost:8765/emu
//
// /emu      bedieningspagina: het ronde scherm (muis = vinger), sensoren, accu, Wi-Fi, BLE, geluid,
//           en ernaast de webinterface (via "Wi-Fi") of de PWA (via een nagebootste Bluetooth-koppeling)
// /         de echte webinterface (firmware/web/index.html, rechtstreeks van schijf: wijzigen + herladen)
// /api/...  dezelfde endpoints als het apparaat (cmd, status, wifi, update)
// /emu/...  scherm, invoer, geluid en de nagebootste BLE-verbinding (NUS-regelprotocol)
//
// Instellingen en eigen profielen blijven bewaard in .pio/webemu-state/ (net als NVS op het apparaat).

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
uint32_t frameNo = 1;  // telt op bij elk nieuw beeld

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

// ---------- Nagebootst apparaat ----------

struct FakeNet {
  char ssid[33] = "Thuisnetwerk", pass[65] = "";
  bool saved = true;
  uint32_t joinAt = 0;  // >0: bezig met verbinden sinds
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
  std::string offered = "v1.0.1";  // "release" die de nagebootste GitHub aanbiedt (leeg = geen)
  uint32_t checkDoneAt = 0, installStart = 0;
  bool installAfterCheck = false;

  EmuPlatform() {
    dev.wifi = DeviceInfo::WifiMode::Station;
    snprintf(dev.ssid, sizeof(dev.ssid), "%s", net.ssid);
    snprintf(dev.ip, sizeof(dev.ip), "127.0.0.1");
    dev.rssi = -58;
    snprintf(dev.version, sizeof(dev.version), "1.0.0");
    dev.batteryPct = -1;  // USB-stroom
    dev.heapFree = 80 * 1024;
    dev.heapMin = 72 * 1024;
    dev.psramFree = 7700 * 1024;
    blePaired = access((gState + "/ble-paired").c_str(), F_OK) == 0;
  }

  // Instellingen en blobs: één bestand per sleutel (zoals NVS)
  void loadSettings(Settings& s) override {
    std::string d;
    if (readFile(gState + "/settings.bin", d) && d.size() == sizeof(Settings)) memcpy(&s, d.data(), sizeof(Settings));
    dev.bleEnabled = s.ble;
  }
  void saveSettings(const Settings& s) override { writeFile(gState + "/settings.bin", &s, sizeof(s)); }
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
    // Testgevallen: wachtwoord "fout" = verkeerd wachtwoord, netwerk "Weg" = niet bereikbaar
    net.joinFails = !strcmp(net.pass, "fout") || !strcmp(net.ssid, "Weg");
    net.failReason = !strcmp(net.ssid, "Weg") ? "netwerk niet gevonden" : "verkeerd wachtwoord";
    dev.wifi = DeviceInfo::WifiMode::Connecting;
    snprintf(dev.ssid, sizeof(dev.ssid), "%s", s);
    dev.ip[0] = 0;
  }
  const char* scanJson() {
    return "[{\"ssid\":\"Thuisnetwerk\",\"rssi\":-52,\"open\":false},"
           "{\"ssid\":\"Buren 5G\",\"rssi\":-71,\"open\":false},"
           "{\"ssid\":\"Gastnetwerk\",\"rssi\":-80,\"open\":true},"
           "{\"ssid\":\"Weg\",\"rssi\":-90,\"open\":false}]";
  }
  void wifi(const char* act, const char* s, const char* p, char* reply, size_t len) override {
    if (!strcmp(act, "off")) {
      dev.wifi = DeviceInfo::WifiMode::Off;
      dev.ssid[0] = dev.ip[0] = 0;
      snprintf(reply, len, "ok wifi uit");
    } else if (!strcmp(act, "on")) {
      if (net.saved) join(net.ssid, net.pass);
      else setAp();
      snprintf(reply, len, "ok wifi aan");
    } else if (!strcmp(act, "join")) {
      join(s, p);
      snprintf(reply, len, "ok verbinden met %s", s);
    } else if (!strcmp(act, "scan")) {
      snprintf(reply, len, "{\"networks\":%s}", scanJson());
    } else if (!strcmp(act, "forget")) {
      net.saved = false;
      net.ssid[0] = net.pass[0] = 0;
      dev.wifi = DeviceInfo::WifiMode::Off;
      dev.ssid[0] = dev.ip[0] = 0;
      snprintf(reply, len, "ok netwerk vergeten");
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
      snprintf(reply, len, "ok controleren...");
    } else if (!strcmp(act, "install")) {
      if (dev.updateState == UpdateState::Installing) snprintf(reply, len, "bezig met installeren");
      else if (dev.updateState == UpdateState::Available) {
        startInstall();
        snprintf(reply, len, "ok %s installeren...", dev.updateVersion);
      } else {
        startCheck(true);
        snprintf(reply, len, "ok controleren en installeren...");
      }
    } else {
      snprintf(reply, len, "updates=%s status=%s huidig=%s nieuwste=%s intern-vrij=%u", updateModeName(updMode),
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

// ---------- Minimale HTTP-server (één verzoek per verbinding) ----------

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
  // "key=..." of een kale "key" (zoals ?pwa)
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

// ---------- Geluid: wat de synth maakt, gaat in een ringbuffer die de browser ophaalt ----------

constexpr size_t kAudioRing = SoundSynth::kRate * 4;
int16_t audioRing[kAudioRing];
uint64_t audioTotal = 0;  // aantal samples ooit gemaakt

}  // namespace

int main(int argc, char** argv) {
  gArgv = argv;
  gRoot = argc > 1 ? argv[1] : ".";
  int port = argc > 2 ? atoi(argv[2]) : 8765;
  gState = gRoot + "/firmware/.pio/webemu-state";
  mkdir((gRoot + "/firmware/.pio").c_str(), 0755);
  mkdir(gState.c_str(), 0755);
  signal(SIGPIPE, SIG_IGN);

  lv_init();
  lv_tick_set_cb(nowMs);
  lv_display_t* disp = lv_display_create(W, H);
  static uint16_t buf[W * H];  // schermvullend, net als op het apparaat
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

  int srv = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // alleen deze computer
  if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(srv, 16) != 0) {
    fprintf(stderr, "kan poort %d niet openen: %s\n", port, strerror(errno));
    return 1;
  }
  printf("deskbuddy web-emulator: http://localhost:%d/emu\n", port);
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
            // PWA-modus: Bluetooth nabootsen (zelfde NUS-regelprotocol als op het apparaat)
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
              respond(fd, 400, "application/json", "{\"ok\":false,\"reply\":\"ongeldige ssid of wachtwoord\"}");
            } else {
              respond(fd, 200, "application/json", "{\"ok\":true,\"reply\":\"verbinden...\"}");
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
            // Upload via de webinterface: grootte controleren, dan "installeren" (scherm) en herstarten
            bool ok = r.body.size() > 100 * 1024;
            if (ok) {
              snprintf(platform.dev.updateVersion, sizeof(platform.dev.updateVersion), "%s", "");
              platform.startInstall();
            }
            respond(fd, ok ? 200 : 500, "application/json",
                    ok ? "{\"ok\":true,\"reply\":\"bijgewerkt, herstarten...\"}" : "{\"ok\":false,\"reply\":\"update mislukt\"}");
          } else if (P == "/emu/screen") {
            uint32_t since = (uint32_t)atol(arg(r, "f").c_str());
            char extra[96];
            snprintf(extra, sizeof(extra), "X-Frame: %u\r\nX-Bright: %u\r\n", frameNo, platform.brightness);
            if (since == frameNo) respond(fd, 204, "application/octet-stream", "", extra);
            else respond(fd, 200, "application/octet-stream", std::string((const char*)fb, sizeof(fb)), extra);
          } else if (P == "/emu/audio") {
            // PCM16 mono sinds 'pos' (max ~1 s terug)
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
                     "\"sound\":%s}",
                     (int)d.wifi, d.batteryPct, d.charging ? "true" : "false", d.bleEnabled ? "true" : "false",
                     d.bleConnected ? "true" : "false", platform.blePaired ? "true" : "false", pairCode,
                     platform.offered.c_str(), d.version, emotionName(app.face.emotion()), platform.brightness,
                     app.sound.enabled() ? "true" : "false");
            respond(fd, 200, "application/json", reply);
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
            } else if (t == "offer") {
              platform.offered = arg(r, "v");
            }
            respond(fd, 200, "application/json", "{\"ok\":true}");
          } else if (P == "/emu/ble") {
            // Nagebootste NUS-verbinding. a=connect|pair|disconnect|line
            std::string a = arg(r, "a");
            DeviceInfo& d = platform.dev;
            if (a == "connect") {
              if (!d.bleEnabled) respond(fd, 400, "text/plain", "Bluetooth staat uit op de deskbuddy");
              else if (platform.blePaired) {
                d.bleConnected = true;
                respond(fd, 200, "text/plain", "bonded");
              } else {
                pairCode = 100000 + (uint32_t)(rand() % 900000);
                d.blePasskey = pairCode;  // verschijnt op het scherm, net als bij echt koppelen
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
              respond(fd, ok ? 200 : 400, "text/plain", ok ? "ok" : "koppelen mislukt (verkeerde code)");
            } else if (a == "disconnect") {
              d.bleConnected = false;
              d.blePasskey = 0;
              pairCode = 0;
              respond(fd, 200, "text/plain", "ok");
            } else {
              std::string line = r.body;
              while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
              if (!d.bleConnected) respond(fd, 400, "text/plain", "niet verbonden");
              else {
                app.command(line.c_str(), reply, 1200);  // zelfde maximale antwoordlengte als ble.cpp
                respond(fd, 200, "text/plain", std::string(reply) + "\n");
              }
            }
          } else {
            respond(fd, 404, "text/plain", "niet gevonden");
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
    // Geluid in hetzelfde tempo maken als de echte tijd
    size_t n = (size_t)(SoundSynth::kRate / 1000 * dt);
    while (n) {
      int16_t chunk[512];
      size_t k = LV_MIN(n, sizeof(chunk) / 2);
      app.sound.fill(chunk, k);
      for (size_t i = 0; i < k; i++) audioRing[(audioTotal + i) % kAudioRing] = chunk[i];
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
      printf("herstarten...\n");
      fflush(stdout);
      close(srv);
      execv("/proc/self/exe", gArgv);  // net als ESP.restart(): opnieuw beginnen met de bewaarde instellingen
      return 0;
    }
  }
}
