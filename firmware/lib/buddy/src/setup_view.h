#pragma once
// Wi-Fi-setupscherm: QR-code om met de telefoon op het setup-access-point te komen, plus uitleg.

#include <lvgl.h>
#include <stdint.h>

namespace buddy {

class SetupView {
 public:
  void create(lv_obj_t* parent);
  // ssid = naam van het setup-AP, ip = adres van de setup-pagina
  // Stap 1: QR om met het setup-AP te verbinden. Stap 2 (zodra een telefoon verbonden is): QR met de
  // link naar de setup-pagina, omdat niet elke telefoon de captive-portal-pop-up vanzelf opent.
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
  uint32_t hideIn_ = 0;
};

}  // namespace buddy
