#pragma once
// Info-paneel (veeg omlaag): Wi-Fi / access point, BLE, accu en firmwareversie.
// Werkt ook zonder verbinding; dan staat er "niet verbonden" / "uit".

#include <lvgl.h>
#include <stdint.h>

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

  bool sound = true;
  bool mascot = false;     // skin

  int batteryPct = -1;     // -1 = geen accu
  bool charging = false;
  char version[16] = "0.1.0";

  bool operator==(const DeviceInfo& o) const;
};

class InfoView {
 public:
  void create(lv_obj_t* parent);
  void show(uint32_t autoHideMs = 10000);
  void hide();
  bool visible() const { return visible_; }
  void update(const DeviceInfo& info, uint32_t dtMs);
  enum class Hit : uint8_t { None, Wifi, Sound, Skin };
  Hit hit(int x, int y) const;  // schermcoördinaten

 private:
  lv_obj_t* wifiRow_ = nullptr;
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
