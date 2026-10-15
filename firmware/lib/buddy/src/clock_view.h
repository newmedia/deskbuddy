#pragma once
// Klok-overlay: tijd, datum en een accuring langs de rand van het ronde scherm.

#include <lvgl.h>
#include <time.h>

namespace buddy {

class ClockView {
 public:
  void create(lv_obj_t* parent);
  void show(uint32_t autoHideMs = 6000);
  void hide();
  bool visible() const { return visible_; }
  // batteryPct < 0 = geen accu aanwezig
  void update(const struct tm& now, int batteryPct, bool charging, uint32_t dtMs);

 private:
  lv_obj_t* root_ = nullptr;
  lv_obj_t* time_ = nullptr;
  lv_obj_t* date_ = nullptr;
  lv_obj_t* batt_ = nullptr;
  lv_obj_t* battLabel_ = nullptr;
  bool visible_ = false;
  uint32_t hideIn_ = 0;
  int lastMin_ = -1, lastPct_ = -2;
  bool lastCharging_ = false;
};

}  // namespace buddy
