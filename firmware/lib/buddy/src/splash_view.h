#pragma once
// Start screen: the deskbuddy.dev logo with the name, shortly after power-on. Afterwards the
// object is deleted (not just hidden).

#include <lvgl.h>
#include <stdint.h>

namespace buddy {

class SplashView {
 public:
  void create(lv_obj_t* parent, const char* version);
  void show(uint32_t ms = 1500);
  void update();
  bool visible() const { return root_ != nullptr; }

 private:
  lv_obj_t* root_ = nullptr;
  uint32_t start_ = 0, total_ = 0;
  bool started_ = false;
};

}  // namespace buddy
