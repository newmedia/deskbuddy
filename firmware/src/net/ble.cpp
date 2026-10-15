#include "ble.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_random.h>

#include <atomic>

#include "app.h"

namespace ble {

namespace {

// Nordic UART Service: door veel tools en Web Bluetooth-voorbeelden ondersteund.
constexpr const char* kService = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* kRx = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // telefoon -> buddy (commando)
constexpr const char* kTx = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // buddy -> telefoon (antwoord, notify)

struct Msg {
  char line[220];
  uint16_t conn;
};

buddy::App* app = nullptr;
QueueHandle_t queue = nullptr;
NimBLEServer* server = nullptr;
NimBLECharacteristic* tx = nullptr;
bool inited = false, enabled = false;
char devName[24] = "deskbuddy";
std::atomic<uint32_t> passkey{0};   // >0 = tonen op het scherm
std::atomic<uint8_t> connected{0};

// Bericht in stukjes van (MTU - 3) versturen; de client plakt ze aan elkaar tot de '\n'.
void send(uint16_t conn, const char* msg) {
  if (!tx) return;
  size_t mtu = server->getPeerMTU(conn);
  size_t chunk = mtu > 23 ? mtu - 3 : 20;
  String s(msg);
  s += '\n';
  for (size_t i = 0; i < s.length(); i += chunk) {
    size_t n = s.length() - i < chunk ? s.length() - i : chunk;
    tx->notify((const uint8_t*)s.c_str() + i, n, conn);
    delay(4);  // geef de stack even lucht bij lange berichten
  }
}

struct ServerCb : NimBLEServerCallbacks {
  void onConnect(NimBLEServer*, NimBLEConnInfo& info) override {
    connected = server->getConnectedCount();
    NimBLEDevice::startSecurity(info.getConnHandle());  // koppelen vragen (pincode)
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    connected = server->getConnectedCount();
    passkey = 0;
  }
  uint32_t onPassKeyDisplay() override {
    uint32_t k = 100000 + esp_random() % 900000;
    passkey = k;
    Serial.printf("[ble] koppelcode %06lu\n", (unsigned long)k);
    return k;
  }
  void onAuthenticationComplete(NimBLEConnInfo& info) override {
    passkey = 0;
    Serial.printf("[ble] koppelen %s\n", info.isEncrypted() ? "gelukt" : "mislukt");
    if (!info.isEncrypted()) server->disconnect(info.getConnHandle());
  }
};

struct RxCb : NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& info) override {
    if (!info.isEncrypted()) return;  // alleen gekoppelde apparaten
    std::string v = c->getValue();
    Msg m;
    size_t n = v.size() < sizeof(m.line) - 1 ? v.size() : sizeof(m.line) - 1;
    memcpy(m.line, v.data(), n);
    m.line[n] = 0;
    while (n && (m.line[n - 1] == '\n' || m.line[n - 1] == '\r')) m.line[--n] = 0;
    m.conn = info.getConnHandle();
    xQueueSend(queue, &m, 0);  // vol = negeren; de hoofdlus verwerkt hem
  }
};

ServerCb serverCb;
RxCb rxCb;

void init() {
  if (inited) return;
  inited = true;
  NimBLEDevice::init(devName);
  NimBLEDevice::setMTU(247);
  NimBLEDevice::setSecurityAuth(true, true, true);  // bonding, MITM (pincode), secure connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
  server = NimBLEDevice::createServer();
  server->setCallbacks(&serverCb);
  server->advertiseOnDisconnect(true);
  NimBLEService* svc = server->createService(kService);
  NimBLECharacteristic* rx =
      svc->createCharacteristic(kRx, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::WRITE_ENC |
                                         NIMBLE_PROPERTY::WRITE_AUTHEN);
  rx->setCallbacks(&rxCb);
  tx = svc->createCharacteristic(kTx, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->setName(devName);
  adv->addServiceUUID(kService);
  adv->enableScanResponse(true);
}

}  // namespace

void begin(buddy::App& a, bool on, const char* name) {
  app = &a;
  snprintf(devName, sizeof(devName), "%s", name);
  queue = xQueueCreate(4, sizeof(Msg));
  setEnabled(on);
}

void setEnabled(bool on) {
  if (on) {
    init();
    NimBLEDevice::getAdvertising()->start();
  } else if (inited) {
    NimBLEDevice::getAdvertising()->stop();
    for (uint16_t h : server->getPeerDevices()) server->disconnect(h);
    passkey = 0;
  }
  enabled = on;
}

void forgetBonds() {
  if (inited) NimBLEDevice::deleteAllBonds();
}

void loop() {
  if (!queue) return;
  Msg m;
  while (xQueueReceive(queue, &m, 0) == pdTRUE) {
    static char reply[1200];
    app->command(m.line, reply, sizeof(reply));
    send(m.conn, reply);
  }
}

void fill(buddy::DeviceInfo& d) {
  d.bleEnabled = enabled;
  d.bleConnected = connected > 0;
  d.blePasskey = passkey;
  snprintf(d.bleName, sizeof(d.bleName), "%s", devName);
}

}  // namespace ble
