#pragma once
// Info-paneel (veeg omlaag): Wi-Fi / access point, BLE, accu en firmwareversie.
// Werkt ook zonder verbinding; dan staat er "niet verbonden" / "uit".

#include <lvgl.h>
#include <stdint.h>

#include "update.h"

namespace buddy {

struct DeviceInfo {
  enum class WifiMode : uint8_t { Off, Connecting, Station, AccessPoint };
  WifiMode wifi = WifiMode::Off;
  char ssid[33] = "";      // verbonden netwerk, of naam van het eigen AP
  char ip[16] = "";
  int rssi = 0;            // dBm (alleen Station)
  int apClients = 0;       // telefoons verbonden met het setup-AP
  char hostname[32] = "deskbuddy";

  bool bleEnabled = false;
  bool bleConnected = false;
  char bleName[24] = "deskbuddy";
  uint32_t blePasskey = 0;  // >0: koppelcode die op het scherm moet staan

  bool sound = true;
  bool mascot = false;     // basis cartoon
  char lookName[16] = "Robot";

  uint32_t heapFree = 0;     // intern RAM vrij (bytes)
  uint32_t heapMin = 0;      // laagste stand sinds opstarten
  uint32_t psramFree = 0;
  float fps = 0;             // getekende frames per seconde
  float renderMs = 0;        // gemiddelde frametijd (tekenen + draaien + versturen)
  float rotateMs = 0, sendMs = 0;
  float load = 0;            // 0..100: deel van de tijd dat de hoofdlus bezig is
  int batteryPct = -1;     // -1 = geen accu
  bool charging = false;
  char version[16] = "0.1.0";

  UpdateMode updateMode = UpdateMode::Notify;
  UpdateState updateState = UpdateState::Idle;
  uint8_t updateProgress = 0;  // 0..100 tijdens installeren
  char updateVersion[16] = ""; // nieuwste release, bijv. "v0.2.0"

  bool operator==(const DeviceInfo& o) const;
};

class InfoView {
 public:
  void create(lv_obj_t* parent);
  void show(uint32_t autoHideMs = 10000);
  void hide();
  bool visible() const { return visible_; }
  void update(const DeviceInfo& info, uint32_t dtMs);
  enum class Hit : uint8_t { None, Wifi, Ble, Sound, Skin, Update };
  Hit hit(int x, int y) const;  // schermcoördinaten

 private:
  lv_obj_t* wifiRow_ = nullptr;
  lv_obj_t* bleRow_ = nullptr;
  lv_obj_t* row(const char* symbol, lv_obj_t** value, lv_obj_t** sub, lv_obj_t** icon = nullptr);
  static void drawSkinIcon(lv_event_t* e);  // mini-gezichtje van de actieve skin

  lv_obj_t* root_ = nullptr;
  lv_obj_t* wifi_ = nullptr;
  lv_obj_t* wifiSub_ = nullptr;
  lv_obj_t* ble_ = nullptr;
  lv_obj_t* bleSub_ = nullptr;
  lv_obj_t* soundRow_ = nullptr;
  lv_obj_t* soundIcon_ = nullptr;
  lv_obj_t* sound_ = nullptr;
  lv_obj_t* soundSub_ = nullptr;
  lv_obj_t* skinRow_ = nullptr;
  lv_obj_t* skinIcon_ = nullptr;
  lv_obj_t* skin_ = nullptr;
  lv_obj_t* skinSub_ = nullptr;
  lv_obj_t* footer_ = nullptr;   // naam + versie
  lv_obj_t* battery_ = nullptr;  // helemaal onderaan
  DeviceInfo last_;
  bool first_ = true;
  bool visible_ = false;
  uint32_t hideIn_ = 0;
};

}  // namespace buddy
