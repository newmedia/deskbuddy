#pragma once
// Wi-Fi-setupscherm: QR-code om met de telefoon op het setup-access-point te komen, plus uitleg.

#include <lvgl.h>
#include <stdint.h>

namespace buddy {

class SetupView {
 public:
  void create(lv_obj_t* parent);
  // ssid = naam van het setup-AP, ip = adres van de setup-pagina
  void show(const char* ssid, const char* ip, uint32_t autoHideMs = 60000);
  void hide();
  bool visible() const { return visible_; }
  void update(uint32_t dtMs);

 private:
  lv_obj_t* root_ = nullptr;
  lv_obj_t* qr_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* hint_ = nullptr;
  bool visible_ = false;
  uint32_t hideIn_ = 0;
};

}  // namespace buddy
