#pragma once
// Wi-Fi setup screen: QR code to reach the setup access point with the phone, plus instructions.

#include <lvgl.h>
#include <stdint.h>

#include "i18n.h"

namespace buddy {

class SetupView {
 public:
  void create(lv_obj_t* parent);
  // ssid = name of the setup AP, ip = address of the setup page
  // Step 1: QR to connect to the setup AP. Step 2 (once a phone is connected): QR with the
  // link to the setup page, because not every phone opens the captive-portal pop-up by itself.
  void show(const char* ssid, const char* ip, uint32_t autoHideMs = 120000);
  void showLinkStep();
  void hide();
  bool visible() const { return visible_; }
  bool linkStep() const { return linkStep_; }
  void update(uint32_t dtMs);

 private:
  lv_obj_t* root_ = nullptr;
  lv_obj_t* qr_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* hint_ = nullptr;
  bool visible_ = false;
  bool linkStep_ = false;
  char ip_[16] = "";
  char ssid_[33] = "";
  Lang lang_ = Lang::En;
  void retext();  // texts in the current language
  uint32_t hideIn_ = 0;
};

// Bluetooth pairing code: shown large while the phone asks for the PIN.
class PairView {
 public:
  void create(lv_obj_t* parent);
  void show(uint32_t code);
  void hide();
  bool visible() const { return visible_; }

 private:
  lv_obj_t* root_ = nullptr;
  lv_obj_t* code_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* hint_ = nullptr;
  bool visible_ = false;
};

}  // namespace buddy
