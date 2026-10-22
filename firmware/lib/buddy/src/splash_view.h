#pragma once
// Opstartscherm: het deskbuddy-logo met de naam, kort na het inschakelen.

#include <lvgl.h>
#include <stdint.h>

namespace buddy {

class SplashView {
 public:
  void create(lv_obj_t* parent, const char* version);
  void show(uint32_t ms = 1500);
  void update(uint32_t dtMs);
  bool visible() const { return left_ > 0; }
  bool created() const { return root_ != nullptr; }

 private:
  lv_obj_t* root_ = nullptr;
  uint32_t left_ = 0, total_ = 0;
};

}  // namespace buddy
