#include "updater.h"
#include "net.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <freertos/idf_additions.h>
#include <mbedtls/platform.h>
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
volatile bool installAfterCheck = false, installWanted = false;
void (*pump)() = nullptr;
bool (*prepareHook)(bool) = nullptr;
void (*releaseHook)() = nullptr;
bool prepared = false;  // alleen op de hoofdlus gebruikt

// Geheugen vrijmaken voor TLS. false = (automatische) klus nu niet doen.
bool prepare(bool urgent) {
  if (prepared) return true;
  if (prepareHook && !prepareHook(urgent)) return false;
  prepared = true;
  return true;
}
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

// Installeren gebeurt op de hoofdlus (interne stack): flash schrijven mag niet vanuit een taak
// waarvan de stack in PSRAM staat. Via pump() blijft het scherm intussen bijwerken.
void doInstall() {
  state = UpdateState::Installing;
  progress = 0;
  net::updateMemory(true);
  if (pump) pump();
  Serial.printf("[update] installeren %s van %s, intern vrij %u\n", latest, assetUrl,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  BundleClient client;
  client.setTimeout(20);  // s
  HTTPUpdate hu(20000);
  hu.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub stuurt door naar zijn downloadserver
  hu.rebootOnUpdate(false);
  hu.onProgress([](int cur, int total) {
    progress = total > 0 ? (uint8_t)((uint64_t)cur * 100 / total) : 0;
    static uint32_t lastPump = 0;
    if (pump && millis() - lastPump >= 40) {
      lastPump = millis();
      pump();
    }
  });
  t_httpUpdate_return r = hu.update(client, assetUrl, current);
  if (r == HTTP_UPDATE_OK) {
    progress = 100;
    if (pump) pump();
    Serial.println("[update] klaar, herstarten");
    delay(800);
    ESP.restart();
  }
  net::updateMemory(false);
  char msg[48];
  snprintf(msg, sizeof(msg), "installeren mislukt: %s", hu.getLastErrorString().c_str());
  fail(msg);
}

void doCheck() {
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
  if (installAfterCheck) installWanted = true;
}

// Vaste worker voor het controleren, met stack (en TCB) in PSRAM: intern RAM is krap zodra BLE draait.
// Hij leeft zolang het apparaat aan staat; taken met PSRAM-stack kun je beter niet zelf laten stoppen.
TaskHandle_t worker = nullptr;

void workerTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    Serial.printf("[update] controleren, intern vrij %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    doCheck();
    // Volgende automatische controle; na een mislukte poging niet 12 uur wachten.
    nextCheck = millis() + (state == UpdateState::Failed ? kRetry : kInterval);
    busy = false;
  }
}

bool startCheck(bool install, bool urgent) {
  if (busy || state == UpdateState::Installing) return false;
  if (!prepare(urgent)) return false;
  if (!worker && xTaskCreatePinnedToCoreWithCaps(workerTask, "update", 12288, nullptr, 1, &worker, 0,
                                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
    worker = nullptr;
    fail("geen geheugen voor update-taak");
    return false;
  }
  installAfterCheck = install;
  busy = true;
  xTaskNotifyGive(worker);
  return true;
}

}  // namespace

// mbedTLS staat in deze Arduino-build op "alleen intern RAM" (~40 KB per verbinding). Met BLE erbij
// is dat er niet; daarom bij voorkeur PSRAM, met intern RAM als terugval.
void* tlsCalloc(size_t n, size_t size) {
  return heap_caps_calloc_prefer(n, size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void begin(const char* currentVersion) {
  mbedtls_platform_set_calloc_free(tlsCalloc, heap_caps_free);
  current = currentVersion;
  loadMode();
}

void setPump(void (*fn)()) { pump = fn; }

void setHooks(bool (*prep)(bool), void (*rel)()) {
  prepareHook = prep;
  releaseHook = rel;
}

void loop(bool online) {
  if (!online) {
    scheduled = false;
    return;
  }
  if (installWanted && !busy) {
    installWanted = false;
    prepare(true);
    doInstall();  // blokkeert tot herstart, of tot het mislukt
    nextCheck = millis() + kRetry;
  }
  if (prepared && !busy && !installWanted) {  // klus klaar: BLE e.d. weer aan
    prepared = false;
    if (releaseHook) releaseHook();
  }
  uint32_t now = millis();
  if (!scheduled) {
    scheduled = true;
    nextCheck = now + kFirstCheck;
  }
  if (mode == UpdateMode::Off || busy || (int32_t)(now - nextCheck) < 0) return;
  // Lukt starten niet (bijv. BLE in gebruik), dan over 30 min opnieuw; anders zet de worker de tijd.
  nextCheck = now + kRetry;
  startCheck(mode == UpdateMode::Auto, false);
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
    if (m == UpdateMode::Auto && state == UpdateState::Available) {
      installWanted = true;
      snprintf(reply, len, "ok updates=auto, %s wordt geinstalleerd", latest);
    }
  } else if (!strcmp(act, "check")) {
    if (startCheck(false, true)) snprintf(reply, len, "ok controleren...");
    else if (busy || state == UpdateState::Installing) snprintf(reply, len, "bezig, probeer zo opnieuw");
    else snprintf(reply, len, "mislukt: %s", error);
  } else if (!strcmp(act, "install")) {
    if (state == UpdateState::Installing || installWanted) {
      snprintf(reply, len, "bezig met installeren");
    } else if (state == UpdateState::Available && !busy) {
      installWanted = true;
      snprintf(reply, len, "ok %s installeren...", latest);
    } else if (startCheck(true, true)) {
      snprintf(reply, len, "ok controleren en installeren...");
    } else if (busy) {
      snprintf(reply, len, "bezig, probeer zo opnieuw");
    } else {
      snprintf(reply, len, "mislukt: %s", error);
    }
  } else {
    snprintf(reply, len, "updates=%s status=%s huidig=%s nieuwste=%s intern-vrij=%u%s%s", buddy::updateModeName(mode),
             buddy::updateStateName(state), current, latest[0] ? latest : "-",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), error[0] ? " fout=" : "", error);
  }
}

}  // namespace net::updater
