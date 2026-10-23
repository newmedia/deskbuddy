#pragma once
// Info panel (swipe down): Wi-Fi / access point, BLE, battery and firmware version.
// Also works without a connection; then it shows "not connected" / "off".

#include <lvgl.h>
#include <stdint.h>

#include "i18n.h"
#include "update.h"

namespace buddy {

struct DeviceInfo {
  enum class WifiMode : uint8_t { Off, Connecting, Station, AccessPoint };
  WifiMode wifi = WifiMode::Off;
  char ssid[33] = "";      // connected network, or name of the own AP
  char ip[16] = "";
  int rssi = 0;            // dBm (Station only)
  int apClients = 0;       // phones connected to the setup AP
  char hostname[32] = "deskbuddy";

  bool bleEnabled = false;
  bool bleConnected = false;
  char bleName[24] = "deskbuddy";
  uint32_t blePasskey = 0;  // >0: pairing code that must be shown on the screen

  bool sound = true;
  bool mascot = false;     // base cartoon
  char lookName[16] = "Robot";

  uint32_t heapFree = 0;     // internal RAM free (bytes)
  uint32_t heapMin = 0;      // lowest value since boot
  uint32_t psramFree = 0;
  float fps = 0;             // drawn frames per second
  float renderMs = 0;        // average frame time (draw + rotate + send)
  float rotateMs = 0, sendMs = 0;
  float load = 0;            // 0..100: fraction of the time the main loop is busy
  int batteryPct = -1;     // -1 = no battery
  bool charging = false;
  char version[16] = "0.1.0";

  UpdateMode updateMode = UpdateMode::Notify;
  UpdateState updateState = UpdateState::Idle;
  uint8_t updateProgress = 0;  // 0..100 during installation
  char updateVersion[16] = ""; // latest release, e.g. "v0.2.0"

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
  Hit hit(int x, int y) const;  // screen coordinates

 private:
  lv_obj_t* wifiRow_ = nullptr;
  lv_obj_t* bleRow_ = nullptr;
  lv_obj_t* row(const char* symbol, lv_obj_t** value, lv_obj_t** sub, lv_obj_t** icon = nullptr);
  static void drawSkinIcon(lv_event_t* e);  // mini face of the active skin

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
  lv_obj_t* logo_ = nullptr;  // small logo left of the version line
  lv_obj_t* footer_ = nullptr;   // name + version
  lv_obj_t* battery_ = nullptr;  // at the very bottom
  DeviceInfo last_;
  bool first_ = true;
  Lang lang_ = Lang::En;  // language of the texts shown now
  bool visible_ = false;
  uint32_t hideIn_ = 0;
};

}  // namespace buddy
