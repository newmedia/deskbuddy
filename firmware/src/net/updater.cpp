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
// Each board has its own asset: an XL must never install the Original's firmware (and vice versa).
#ifdef DESKBUDDY_BOARD_XL
constexpr const char* kAsset = "/firmware-xl.bin";
#else
constexpr const char* kAsset = "/firmware.bin";
#endif
constexpr uint32_t kFirstCheck = 30000;              // wait a bit after connecting (NTP, settle)
constexpr uint32_t kInterval = 12UL * 3600 * 1000;   // then every 12 hours
constexpr uint32_t kRetry = 30UL * 60 * 1000;        // retry 30 min after a failed check

// NetworkClientSecure with the CA bundle that ships with ESP-IDF (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE):
// covers api.github.com and the download servers GitHub redirects to.
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
bool prepared = false;  // only used on the main loop

// Free up memory for TLS. false = don't do the (automatic) job now.
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

// Reads the (large) release JSON as a stream and keeps only tag_name and the download URL of
// firmware.bin. Small tokenizer: strings, keys (string followed by ':') and their value.
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
          if (tag[0] && url[0]) return true;  // we don't need the rest (e.g. the release notes)
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
      valueNext = false;  // number, object, list or true/false/null
    }
  }
  return tag[0] != 0;
}

// Installing happens on the main loop (internal stack): writing flash must not happen from a task
// whose stack is in PSRAM. pump() keeps the screen updating in the meantime.
void doInstall() {
  state = UpdateState::Installing;
  progress = 0;
  net::updateMemory(true);
  if (pump) pump();
  Serial.printf("[update] installing %s from %s, internal free %u\n", latest, assetUrl,
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  BundleClient client;
  client.setTimeout(20);  // s
  HTTPUpdate hu(20000);
  hu.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);  // GitHub redirects to its download server
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
    Serial.println("[update] done, restarting");
    delay(800);
    ESP.restart();
  }
  net::updateMemory(false);
  char msg[48];
  snprintf(msg, sizeof(msg), "install failed: %s", hu.getLastErrorString().c_str());
  fail(msg);
}

void doCheck() {
  state = UpdateState::Checking;
  error[0] = 0;
  BundleClient client;
  client.setTimeout(15);
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding: we read the stream ourselves
  http.setTimeout(15000);
  http.setUserAgent(String("deskbuddy/") + current);  // GitHub rejects requests without a User-Agent
  if (!http.begin(client, kLatestUrl)) {
    fail("cannot reach GitHub");
    return;
  }
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code != 200) {
    http.end();
    if (code == 404) {  // no release yet
      latest[0] = 0;
      state = UpdateState::UpToDate;
      return;
    }
    char msg[48];
    snprintf(msg, sizeof(msg), "GitHub replied %d", code);
    fail(msg);
    return;
  }
  char tag[16], url[200];
  bool ok = parseRelease(*http.getStreamPtr(), tag, sizeof(tag), url, sizeof(url));
  http.end();
  if (!ok) {
    fail("cannot read response");
    return;
  }
  snprintf(latest, sizeof(latest), "%s", tag);
  if (buddy::compareVersions(tag, current) <= 0) {
    Serial.printf("[update] up to date (%s, latest release %s)\n", current, tag);
    state = UpdateState::UpToDate;
    return;
  }
  if (!url[0]) {
    char msg[48];
    snprintf(msg, sizeof(msg), "release has no %s", kAsset + 1);
    fail(msg);
    return;
  }
  snprintf(assetUrl, sizeof(assetUrl), "%s", url);
  Serial.printf("[update] new version %s available (now %s)\n", tag, current);
  state = UpdateState::Available;
  if (installAfterCheck) installWanted = true;
}

// Fixed worker for checking, with stack (and TCB) in PSRAM: internal RAM is tight once BLE runs.
// It lives as long as the device is on; tasks with a PSRAM stack are best not left to stop themselves.
TaskHandle_t worker = nullptr;

void workerTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    Serial.printf("[update] checking, internal free %u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    doCheck();
    // Next automatic check; after a failed attempt don't wait 12 hours.
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
    fail("no memory for update task");
    return false;
  }
  installAfterCheck = install;
  busy = true;
  xTaskNotifyGive(worker);
  return true;
}

}  // namespace

// mbedTLS is set to "internal RAM only" in this Arduino build (~40 KB per connection). With BLE
// running that isn't available; so prefer PSRAM, with internal RAM as fallback.
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
    doInstall();  // blocks until restart, or until it fails
    nextCheck = millis() + kRetry;
  }
  if (prepared && !busy && !installWanted) {  // job done: BLE etc. back on
    prepared = false;
    if (releaseHook) releaseHook();
  }
  uint32_t now = millis();
  if (!scheduled) {
    scheduled = true;
    nextCheck = now + kFirstCheck;
  }
  if (mode == UpdateMode::Off || busy || (int32_t)(now - nextCheck) < 0) return;
  // If starting fails (e.g. BLE in use), retry in 30 min; otherwise the worker sets the time.
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
      snprintf(reply, len, "ok updates=auto, installing %s", latest);
    }
  } else if (!strcmp(act, "check")) {
    if (startCheck(false, true)) snprintf(reply, len, "ok checking...");
    else if (busy || state == UpdateState::Installing) snprintf(reply, len, "busy, try again shortly");
    else snprintf(reply, len, "failed: %s", error);
  } else if (!strcmp(act, "install")) {
    if (state == UpdateState::Installing || installWanted) {
      snprintf(reply, len, "installing in progress");
    } else if (state == UpdateState::Available && !busy) {
      installWanted = true;
      snprintf(reply, len, "ok installing %s...", latest);
    } else if (startCheck(true, true)) {
      snprintf(reply, len, "ok checking and installing...");
    } else if (busy) {
      snprintf(reply, len, "busy, try again shortly");
    } else {
      snprintf(reply, len, "failed: %s", error);
    }
  } else {
    snprintf(reply, len, "updates=%s status=%s current=%s latest=%s internal-free=%u%s%s", buddy::updateModeName(mode),
             buddy::updateStateName(state), current, latest[0] ? latest : "-",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), error[0] ? " error=" : "", error);
  }
}

}  // namespace net::updater
