#include "updater.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <ssl_client.h>

#include "info_view.h"
#include "update.h"

namespace net::updater {

namespace {

using buddy::UpdateMode;
using buddy::UpdateState;

constexpr const char* kLatestUrl = "https://api.github.com/repos/newmedia/deskbuddy/releases/latest";
constexpr const char* kAsset = "/firmware.bin";
constexpr uint32_t kFirstCheck = 30000;              // na verbinden even wachten (NTP, rust)
constexpr uint32_t kInterval = 12UL * 3600 * 1000;   // daarna elke 12 uur
constexpr uint32_t kRetry = 30UL * 60 * 1000;        // na een mislukte controle over 30 min opnieuw

// NetworkClientSecure met de CA-bundel die in ESP-IDF zit (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE):
// dekt api.github.com en de downloadservers waar GitHub naar doorstuurt.
class BundleClient : public NetworkClientSecure {
 public:
  BundleClient() {
    attach_ssl_certificate_bundle(sslclient.get(), true);
    _use_ca_bundle = true;
  }
};

const char* current = "0.0.0";
UpdateMode mode = UpdateMode::Notify;
volatile UpdateState state = UpdateState::Idle;
volatile uint8_t progress = 0;
volatile bool busy = false;
char latest[16] = "";
char assetUrl[200] = "";
char error[48] = "";
volatile uint32_t nextCheck = 0;
bool scheduled = false;

void loadMode() {
  Preferences p;
  p.begin("update", true);
  mode = (UpdateMode)p.getUChar("mode", (uint8_t)UpdateMode::Notify);
  p.end();
  if (mode > UpdateMode::Auto) mode = UpdateMode::Notify;
}

void saveMode() {
  Preferences p;
  p.begin("update", false);
  p.putUChar("mode", (uint8_t)mode);
  p.end();
}

void fail(const char* msg) {
  snprintf(error, sizeof(error), "%s", msg);
  Serial.printf("[update] %s\n", msg);
  state = UpdateState::Failed;
}

// Leest het (grote) release-JSON als stroom en onthoudt alleen tag_name en de download-URL van
// firmware.bin. Kleine tokenizer: strings, sleutels (string gevolgd door ':') en hun waarde.
bool parseRelease(NetworkClient& in, char* tag, size_t tagLen, char* url, size_t urlLen) {
  char key[24] = "", buf[200];
  size_t n = 0;
  bool inStr = false, esc = false, haveStr = false, valueNext = false, truncated = false;
  tag[0] = url[0] = 0;
  uint32_t last = millis();
  while (millis() - last < 8000) {
    int c = in.read();
    if (c < 0) {
      if (!in.available() && !in.connected()) break;
      delay(5);
      continue;
    }
    last = millis();
    if (inStr) {
      if (esc) esc = false;
      else if (c == '\\') { esc = true; continue; }
      else if (c == '"') {
        inStr = false;
        buf[n] = 0;
        if (valueNext) {
          valueNext = false;
          if (!truncated && !strcmp(key, "tag_name") && !tag[0]) snprintf(tag, tagLen, "%s", buf);
          if (!truncated && !strcmp(key, "browser_download_url") && n > strlen(kAsset) &&
              !strcmp(buf + n - strlen(kAsset), kAsset))
            snprintf(url, urlLen, "%s", buf);
          if (tag[0] && url[0]) return true;  // de rest (o.a. de release-tekst) hoeven we niet
        } else {
          haveStr = true;
        }
        continue;
      }
      if (n < sizeof(buf) - 1) buf[n++] = (char)c;
      else truncated = true;
      continue;
    }
    if (c == '"') {
      inStr = true;
      n = 0;
      truncated = false;
    } else if (c == ':' && haveStr) {
      buf[n] = 0;
      snprintf(key, sizeof(key), "%s", truncated ? "" : buf);
      valueNext = true;
      haveStr = false;
    } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
      haveStr = false;
      valueNext = false;  // getal, object, lijst of true/false/null
    }
  }
  return tag[0] != 0;
}

void doInstall() {
  state = UpdateState::Installing;
  progress = 0;
  Serial.printf("[update] installeren %s van %s\n", latest, assetUrl);
  BundleClient client;
  client.setTimeout(20);  // s
  HTTPUpdate hu(20000);
  hu.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub stuurt door naar zijn downloadserver
  hu.rebootOnUpdate(false);
  hu.onProgress([](int cur, int total) { progress = total > 0 ? (uint8_t)((uint64_t)cur * 100 / total) : 0; });
  t_httpUpdate_return r = hu.update(client, assetUrl, current);
  if (r == HTTP_UPDATE_OK) {
    progress = 100;
    Serial.println("[update] klaar, herstarten");
    delay(800);
    ESP.restart();
  }
  char msg[48];
  snprintf(msg, sizeof(msg), "installeren mislukt: %s", hu.getLastErrorString().c_str());
  fail(msg);
}

void doCheck(bool installAfter) {
  state = UpdateState::Checking;
  error[0] = 0;
  BundleClient client;
  client.setTimeout(15);
  HTTPClient http;
  http.useHTTP10(true);  // geen chunked encoding: we lezen de stroom zelf
  http.setTimeout(15000);
  http.setUserAgent(String("deskbuddy/") + current);  // GitHub weigert verzoeken zonder User-Agent
  if (!http.begin(client, kLatestUrl)) {
    fail("kan GitHub niet bereiken");
    return;
  }
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code != 200) {
    http.end();
    if (code == 404) {  // nog geen enkele release
      latest[0] = 0;
      state = UpdateState::UpToDate;
      return;
    }
    char msg[48];
    snprintf(msg, sizeof(msg), "GitHub antwoordt %d", code);
    fail(msg);
    return;
  }
  char tag[16], url[200];
  bool ok = parseRelease(*http.getStreamPtr(), tag, sizeof(tag), url, sizeof(url));
  http.end();
  if (!ok) {
    fail("antwoord niet te lezen");
    return;
  }
  snprintf(latest, sizeof(latest), "%s", tag);
  if (buddy::compareVersions(tag, current) <= 0) {
    Serial.printf("[update] up-to-date (%s, nieuwste release %s)\n", current, tag);
    state = UpdateState::UpToDate;
    return;
  }
  if (!url[0]) {
    fail("release heeft geen firmware.bin");
    return;
  }
  snprintf(assetUrl, sizeof(assetUrl), "%s", url);
  Serial.printf("[update] nieuwe versie %s beschikbaar (nu %s)\n", tag, current);
  state = UpdateState::Available;
  if (installAfter) doInstall();
}

enum class Job : uint8_t { Check, CheckAndInstall, Install };

void task(void* arg) {
  Job job = (Job)(uintptr_t)arg;
  Serial.printf("[update] start, heap vrij %u\n", (unsigned)ESP.getFreeHeap());
  if (job == Job::Install) doInstall();
  else doCheck(job == Job::CheckAndInstall);
  // Volgende automatische controle; na een mislukte poging niet 12 uur wachten.
  nextCheck = millis() + (state == UpdateState::Failed ? kRetry : kInterval);
  busy = false;
  vTaskDelete(nullptr);
}

bool start(Job job) {
  if (busy) return false;
  busy = true;
  // TLS heeft een flinke stack nodig; core 0 zodat de UI-lus op core 1 vloeiend blijft.
  if (xTaskCreatePinnedToCore(task, "update", 12288, (void*)(uintptr_t)job, 1, nullptr, 0) != pdPASS) {
    busy = false;
    fail("geen geheugen voor update-taak");
    return false;
  }
  return true;
}

}  // namespace

void begin(const char* currentVersion) {
  current = currentVersion;
  loadMode();
}

void loop(bool online) {
  if (!online) {
    scheduled = false;
    return;
  }
  uint32_t now = millis();
  if (!scheduled) {
    scheduled = true;
    nextCheck = now + kFirstCheck;
  }
  if (mode == UpdateMode::Off || busy || (int32_t)(now - nextCheck) < 0) return;
  nextCheck = now + kRetry;  // als vangnet; de taak zet de echte volgende tijd
  start(mode == UpdateMode::Auto ? Job::CheckAndInstall : Job::Check);
}

void fill(buddy::DeviceInfo& d) {
  d.updateMode = mode;
  d.updateState = state;
  d.updateProgress = progress;
  snprintf(d.updateVersion, sizeof(d.updateVersion), "%s", latest);
}

void action(const char* act, char* reply, size_t len) {
  UpdateMode m;
  if (buddy::updateModeFromName(act, m)) {
    mode = m;
    saveMode();
    snprintf(reply, len, "ok updates=%s", buddy::updateModeName(m));
    if (m == UpdateMode::Auto && state == UpdateState::Available && start(Job::Install))
      snprintf(reply, len, "ok updates=auto, %s wordt geinstalleerd", latest);
  } else if (!strcmp(act, "check")) {
    if (start(Job::Check)) snprintf(reply, len, "ok controleren...");
    else snprintf(reply, len, "bezig, probeer zo opnieuw");
  } else if (!strcmp(act, "install")) {
    if (state == UpdateState::Available && start(Job::Install)) snprintf(reply, len, "ok %s installeren...", latest);
    else if (busy) snprintf(reply, len, "bezig, probeer zo opnieuw");
    else if (start(Job::CheckAndInstall)) snprintf(reply, len, "ok controleren en installeren...");
    else snprintf(reply, len, "kan nu niet");
  } else {
    snprintf(reply, len, "updates=%s status=%s huidig=%s nieuwste=%s%s%s", buddy::updateModeName(mode),
             buddy::updateStateName(state), current, latest[0] ? latest : "-", error[0] ? " fout=" : "", error);
  }
}

}  // namespace net::updater
