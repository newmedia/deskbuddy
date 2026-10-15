#include "net.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_netif.h>
#include <esp_sntp.h>

#include "app.h"
#include "board/board.h"
#include "updater.h"
#include "web_index.h"  // gegenereerd door scripts/embed_web.py uit web/index.html

namespace net {

namespace {

using Mode = buddy::DeviceInfo::WifiMode;

constexpr const char* kHost = "deskbuddy";
constexpr const char* kTz = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr uint32_t kConnectTimeout = 20000;   // ms voordat we bij een eerste setup naar AP gaan
constexpr uint32_t kApIdleTimeout = 600000;   // setup-AP gaat na 10 min zonder bezoekers uit

buddy::App* app = nullptr;
WebServer server(80);
DNSServer dns;
Preferences prefs;

Mode mode = Mode::Off;
char ssid[33] = "", pass[65] = "";
char apName[24] = "";
uint32_t modeSince = 0, apLastClient = 0;
bool servicesUp = false, apUp = false, timeSynced = false, otaBusy = false;
uint8_t otaPct = 0;
void (*pumpFn)() = nullptr;  // houdt het scherm (matrix + ring) in beweging tijdens een blokkerende upload

void pumpNow() {
  static uint32_t last = 0;
  if (pumpFn && millis() - last >= 100) {  // ~10 fps: genoeg voor de animatie, upload krijgt voorrang
    last = millis();
    pumpFn();
  }
}
// Verbindingspoging (voor terugkoppeling in de setup-pagina)
bool joining = false;
uint32_t joinStart = 0, staSince = 0;
int lastReason = 0;

const char* reasonText(int r) {
  switch (r) {
    case 2: case 15: case 202: case 204: return "wachtwoord klopt niet";
    case 201: return "netwerk niet gevonden (alleen 2.4 GHz wordt ondersteund)";
    case 205: return "verbinding mislukt";
    case 0: return "";
    default: return "verbinding verbroken";
  }
}

void loadCreds() {
  prefs.begin("wifi", false);
  if (prefs.isKey("ssid")) prefs.getString("ssid", ssid, sizeof(ssid));
  if (prefs.isKey("pass")) prefs.getString("pass", pass, sizeof(pass));
  prefs.end();
}

void saveCreds() {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void setMode(Mode m) {
  mode = m;
  modeSince = millis();
}

// ---------- web ----------

void jsonEscape(String& out, const char* s) {
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    if ((uint8_t)*s >= 0x20) out += *s;
  }
}

void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

// Debug: in setup-modus elk verzoek loggen (serial), om captive-portal-gedrag te kunnen volgen.
void logReq(const char* what) {
  if (!apUp) return;
  Serial.printf("[web] %s %s%s -> %s  UA=\"%s\" Accept=\"%s\"\n", server.method() == HTTP_GET ? "GET" : "POST",
                server.hostHeader().c_str(), server.uri().c_str(), what, server.header("User-Agent").c_str(),
                server.header("Accept").c_str());
}

bool captiveRedirect() {
  // In AP-modus elk onbekend domein naar de setup-pagina sturen (captive portal).
  if (!apUp) return false;
  // Alleen verzoeken die via het setup-netwerk binnenkomen; via het thuisnetwerk gewoon de pagina.
  if (server.client().localIP() != WiFi.softAPIP()) return false;
  String host = server.hostHeader();
  if (host == WiFi.softAPIP().toString() || host.startsWith(kHost)) return false;
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
  return true;
}

void sendCaptiveJson() {
  String ip = WiFi.softAPIP().toString();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/captive+json",
              String("{\"captive\":") + (apUp ? "true" : "false") + ",\"user-portal-url\":\"http://" + ip + "/\"}");
}

void handleIndex() {
  logReq("index");
  if (captiveRedirect()) return;
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", (const char*)kIndexGz, kIndexGzLen);
}

void handleStatus() {
  char buf[512];
  app->statusJson(buf, sizeof(buf));
  sendJson(200, buf);
}

void handleCmd() {
  String line = server.hasArg("c") ? server.arg("c") : server.arg("plain");
  line.trim();
  char reply[400];
  bool ok = app->command(line.c_str(), reply, sizeof(reply));
  String body = String("{\"ok\":") + (ok ? "true" : "false") + ",\"reply\":\"";
  jsonEscape(body, reply);
  body += "\"}";
  sendJson(ok ? 200 : 400, body);
}

void handleScan() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    sendJson(200, "{\"scanning\":true}");
    return;
  }
  if (n == WIFI_SCAN_RUNNING) {
    sendJson(200, "{\"scanning\":true}");
    return;
  }
  String body = "{\"scanning\":false,\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i) body += ',';
    body += "{\"ssid\":\"";
    jsonEscape(body, WiFi.SSID(i).c_str());
    body += "\",\"rssi\":" + String(WiFi.RSSI(i)) +
            ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
  }
  body += "]}";
  WiFi.scanDelete();
  sendJson(200, body);
}

void handleWifiPost() {
  String s = server.arg("ssid"), p = server.arg("pass");
  if (s.isEmpty() || s.length() > 32 || p.length() > 64) {
    sendJson(400, "{\"ok\":false,\"reply\":\"ongeldige ssid of wachtwoord\"}");
    return;
  }
  sendJson(200, "{\"ok\":true,\"reply\":\"verbinden...\"}");
  delay(100);  // antwoord eerst versturen
  join(s.c_str(), p.c_str());
}

void handleWifiState() {
  const char* st = "idle";
  bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) st = "connected";
  else if (joining && (millis() - joinStart > 25000 || (lastReason && lastReason != 8))) st = "failed";
  else if (joining) st = "connecting";
  String body = String("{\"sta\":\"") + st + "\",\"ssid\":\"";
  jsonEscape(body, ssid);
  body += "\",\"ip\":\"" + (connected ? WiFi.localIP().toString() : String("")) + "\",\"reason\":\"";
  jsonEscape(body, connected ? "" : (millis() - joinStart > 25000 && !lastReason ? "geen antwoord van het netwerk"
                                                                                    : reasonText(lastReason)));
  body += "\",\"switchIn\":" + String(connected && apUp && staSince ? (30 - (int)((millis() - staSince) / 1000)) : 0) + "}";
  sendJson(200, body);
}

void handleUpdateDone() {
  bool ok = !Update.hasError();
  sendJson(ok ? 200 : 500, ok ? "{\"ok\":true,\"reply\":\"bijgewerkt, herstarten...\"}"
                              : "{\"ok\":false,\"reply\":\"update mislukt\"}");
  if (ok) {
    delay(300);
    ESP.restart();
  }
  otaBusy = false;
}

void handleUpdateUpload() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaBusy = true;
    otaPct = 0;
    Update.begin(UPDATE_SIZE_UNKNOWN);
    pumpNow();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
    size_t total = server.clientContentLength();
    if (total) otaPct = (uint8_t)LV_MIN(99u, (unsigned)((uint64_t)up.totalSize * 100 / total));
    pumpNow();
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaBusy = false;
  }
}

void startServices() {
  if (servicesUp) return;
  servicesUp = true;
  static const char* hdrs[] = {"User-Agent", "Accept"};
  server.collectHeaders(hdrs, 2);
  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/captive", HTTP_GET, [] {
    logReq("captive-api");
    sendCaptiveJson();
  });
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/wifi/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifiPost);
  server.on("/api/wifi/state", HTTP_GET, handleWifiState);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  // Controle-URL's van Android, iOS/macOS en Windows: doorsturen = "er is een portal".
  for (const char* probe : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
                            "/connecttest.txt", "/ncsi.txt", "/redirect", "/canonical.html", "/success.txt"}) {
    server.on(probe, HTTP_GET, [] {
      logReq("probe");
      if (apUp) {
        server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
        server.send(302, "text/plain", "");
      } else {
        server.send(204, "text/plain", "");
      }
    });
  }
  server.onNotFound([] {
    logReq("notfound");
    if (!captiveRedirect()) server.send(404, "text/plain", "niet gevonden");
  });
  server.begin();
}

void startStationServices() {
  MDNS.end();
  if (MDNS.begin(kHost)) MDNS.addService("http", "tcp", 80);
  ArduinoOTA.setHostname(kHost);
  ArduinoOTA.setMdnsEnabled(false);  // mDNS doen we zelf (zie hierboven)
  ArduinoOTA.onStart([] {
    otaBusy = true;
    otaPct = 0;
  });
  ArduinoOTA.onProgress([](unsigned int cur, unsigned int total) {
    otaPct = total ? (uint8_t)((uint64_t)cur * 100 / total) : 0;
    pumpNow();
  });
  ArduinoOTA.onEnd([] { otaBusy = false; });
  ArduinoOTA.onError([](ota_error_t) { otaBusy = false; });
  ArduinoOTA.begin();
  configTzTime(kTz, "pool.ntp.org", "time.google.com");
  timeSynced = false;
}

void startAp() {
  if (apUp) return;
  WiFi.mode(WIFI_AP_STA);  // STA erbij: nodig voor scannen en om het thuisnetwerk te blijven proberen
  WiFi.softAP(apName);
  // Laat DHCP onszelf als DNS-server meegeven (staat in IDF 5 standaard uit). Zonder DNS kan een
  // telefoon de controle-URL's niet opzoeken en denkt hij alleen "geen internet".
  const IPAddress apIp(192, 168, 4, 1);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0), IPAddress(192, 168, 4, 2), apIp);
  // Bewust GEEN DHCP-optie 114 (RFC 8910): Android behandelt die als capport-API (RFC 8908, moet https
  // zijn) en slaat dan soms zijn gewone HTTP-probe over. Zonder optie 114 doet Android de klassieke
  // probe (generate_204) via onze DNS, en die sturen we door naar de setup-pagina.
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());
  apUp = true;
  apLastClient = millis();
  setMode(Mode::AccessPoint);
}

void stopAp() {
  if (!apUp) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  apUp = false;
}

void connectSta() {
  if (!ssid[0]) return;
  if (!apUp) WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // geen modem-sleep: veel lagere latency (web-UI, OTA-uploads); kost wat stroom
  WiFi.setHostname(kHost);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, pass);
  if (!apUp) setMode(Mode::Connecting);
}

}  // namespace

void begin(buddy::App& a) {
  app = &a;
  WiFi.persistent(false);  // inloggegevens bewaren we zelf in NVS
  WiFi.onEvent([](arduino_event_id_t e, arduino_event_info_t info) {
    if (e == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("[ap] apparaat verbonden");
    if (e == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("[ap] apparaat weg");
    if (e == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      lastReason = info.wifi_sta_disconnected.reason;
      Serial.printf("[sta] verbinding mislukt/verbroken, reden %d (%s)\n", lastReason, reasonText(lastReason));
    }
    if (e == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      lastReason = 0;
      Serial.printf("[sta] verbonden, IP %s\n", IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
    }
    if (e == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED)
      Serial.printf("[ap] IP uitgedeeld: %s\n", IPAddress(info.wifi_ap_staipassigned.ip.addr).toString().c_str());
  });
  loadCreds();
  // Eerst Wi-Fi (en daarmee de lwIP-stack) starten; pas daarna kan de webserver een socket openen.
  WiFi.mode(ssid[0] ? WIFI_STA : WIFI_AP_STA);
  uint64_t mac = ESP.getEfuseMac();  // vaste fabrieks-MAC (WiFi.macAddress() is hier nog leeg)
  snprintf(apName, sizeof(apName), "deskbuddy-%02X%02X", (unsigned)((mac >> 32) & 0xff),
           (unsigned)((mac >> 40) & 0xff));
  if (ssid[0]) connectSta();
  else startAp();  // eerste keer: direct setup aanbieden (gaat vanzelf uit na 10 min)
  startServices();
}

void loop() {
  if (mode == Mode::Off) return;
  if (servicesUp) server.handleClient();
  if (apUp) dns.processNextRequest();

  bool connected = WiFi.status() == WL_CONNECTED;
  uint32_t now = millis();

  switch (mode) {
    case Mode::Connecting:
      if (connected) {
        joining = false;
        setMode(Mode::Station);
        startStationServices();
      } else if (now - modeSince > kConnectTimeout && !apUp) {
        startAp();  // STA blijft het op de achtergrond proberen
      }
      break;
    case Mode::AccessPoint:
      if (WiFi.softAPgetStationNum() > 0) apLastClient = now;
      if (!connected) staSince = 0;
      if (connected) {
        // Verbonden met het thuisnetwerk: AP nog 30 s laten staan zodat de telefoon het succes ziet.
        if (!staSince) staSince = now;
        if (now - staSince > 30000) {
          staSince = 0;
          joining = false;
          stopAp();
          WiFi.mode(WIFI_STA);
          setMode(Mode::Station);
          startStationServices();
        }
      } else if (now - apLastClient > kApIdleTimeout) {
        stopAp();
        if (ssid[0]) connectSta();
        else off();
      }
      break;
    case Mode::Station:
      ArduinoOTA.handle();
      updater::loop(connected);
      if (!timeSynced && esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        timeSynced = true;
        board::rtcWrite(time(nullptr));
      }
      if (!connected) setMode(Mode::Connecting);  // autoReconnect regelt de rest
      break;
    default:
      break;
  }
}

void setPump(void (*fn)()) { pumpFn = fn; }

void fill(buddy::DeviceInfo& d) {
  updater::fill(d);
  if (otaBusy) {  // upload via web of ArduinoOTA: zelfde weergave als een GitHub-update
    d.updateState = buddy::UpdateState::Installing;
    d.updateProgress = otaPct;
    snprintf(d.updateVersion, sizeof(d.updateVersion), "upload");
  }
  snprintf(d.hostname, sizeof(d.hostname), "%s", kHost);
  d.wifi = mode;
  switch (mode) {
    case Mode::Station:
      snprintf(d.ssid, sizeof(d.ssid), "%s", WiFi.SSID().c_str());
      snprintf(d.ip, sizeof(d.ip), "%s", WiFi.localIP().toString().c_str());
      d.rssi = WiFi.RSSI();
      break;
    case Mode::AccessPoint:
      snprintf(d.ssid, sizeof(d.ssid), "%s", apName);
      snprintf(d.ip, sizeof(d.ip), "%s", WiFi.softAPIP().toString().c_str());
      d.rssi = 0;
      d.apClients = WiFi.softAPgetStationNum();
      break;
    case Mode::Connecting:
      snprintf(d.ssid, sizeof(d.ssid), "%s", ssid);
      d.ip[0] = 0;
      break;
    default:
      d.ssid[0] = d.ip[0] = 0;
      break;
  }
}

void startSetupAp() {
  if (mode == Mode::Station) return;
  startAp();
}

void off() {
  stopAp();
  MDNS.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  setMode(Mode::Off);
}

void join(const char* s, const char* p) {
  joining = true;
  joinStart = millis();
  lastReason = 0;
  staSince = 0;
  snprintf(ssid, sizeof(ssid), "%s", s);
  snprintf(pass, sizeof(pass), "%s", p);
  saveCreds();
  WiFi.disconnect(false);
  connectSta();
}

void action(const char* act, const char* s, const char* p, char* reply, size_t len) {
  if (!strcmp(act, "off")) {
    off();
    snprintf(reply, len, "ok wifi uit");
  } else if (!strcmp(act, "on")) {
    if (ssid[0]) connectSta();
    else startAp();
    snprintf(reply, len, "ok wifi aan");
  } else if (!strcmp(act, "join")) {
    join(s, p);
    snprintf(reply, len, "ok verbinden met %s", s);
  } else if (!strcmp(act, "scan")) {
    // Synchroon zoeken (~2 s): handig via BLE, waar geen asynchrone polling is.
    if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks(false);
    String body = "{\"networks\":[";
    int shown = 0;
    for (int i = 0; i < n && shown < 12; i++) {
      if (WiFi.SSID(i).isEmpty()) continue;
      if (shown++) body += ',';
      body += "{\"ssid\":\"";
      jsonEscape(body, WiFi.SSID(i).c_str());
      body += "\",\"rssi\":" + String(WiFi.RSSI(i)) +
              ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    body += "]}";
    WiFi.scanDelete();
    snprintf(reply, len, "%s", body.c_str());
  } else if (!strcmp(act, "forget")) {
    ssid[0] = pass[0] = 0;
    saveCreds();
    off();
    snprintf(reply, len, "ok netwerk vergeten");
  } else {
    buddy::DeviceInfo d;
    fill(d);
    snprintf(reply, len, "mode=%d ssid=%s ip=%s rssi=%d heap=%u", (int)d.wifi, d.ssid, d.ip, d.rssi,
             (unsigned)ESP.getFreeHeap());
  }
}

}  // namespace net
