#include "espnow.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/idf_additions.h>
#include <freertos/queue.h>

#include "app.h"

namespace espnow {

namespace {

struct Frame {
  uint8_t len;
  int8_t rssi;
  uint8_t data[buddy::Mesh::kMaxFrame];
};

const uint8_t kBroadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
buddy::App* app = nullptr;
QueueHandle_t rxQueue = nullptr;  // in PSRAM: internal RAM is tight
bool on = false;
uint32_t dropped = 0;  // queue full (main loop too slow)

// Wi-Fi task: copy and hand over; the frame is only valid during the callback
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (!rxQueue || len <= 0 || len > (int)buddy::Mesh::kMaxFrame) return;
  Frame f;
  f.len = (uint8_t)len;
  f.rssi = info && info->rx_ctrl ? (int8_t)info->rx_ctrl->rssi : 0;
  memcpy(f.data, data, (size_t)len);
  if (xQueueSend(rxQueue, &f, 0) != pdTRUE) dropped++;
}

void start() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("[espnow] init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, kBroadcast, 6);
  peer.channel = 0;  // the current channel: the router's when connected
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;  // broadcast cannot be encrypted; frames are signed instead (mesh.h)
  esp_now_add_peer(&peer);
  uint8_t ch = 0;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&ch, &second);
  on = true;
  Serial.printf("[espnow] on, channel %u, internal free %u KB\n", ch,
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
}

void stop() {
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  on = false;
  Serial.println("[espnow] off");
}

}  // namespace

void begin(buddy::App& a) {
  app = &a;
  rxQueue = xQueueCreateWithCaps(8, sizeof(Frame), MALLOC_CAP_SPIRAM);
}

void loop() {
  if (!app || !rxQueue) return;
  bool want = app->mesh.enabled() && WiFi.getMode() != WIFI_OFF;
  if (want && !on) start();
  if (!want && on) stop();
  Frame f;
  while (xQueueReceive(rxQueue, &f, 0) == pdTRUE) app->onMeshFrame(f.data, f.len, f.rssi);
}

void send(const uint8_t* data, size_t len) {
  if (on && len <= buddy::Mesh::kMaxFrame) esp_now_send(kBroadcast, data, len);
}

}  // namespace espnow
