#include "splash_view.h"

#include "draw_util.h"

namespace buddy {

namespace {
void drawCb(lv_event_t* e) {
  lv_obj_t* o = (lv_obj_t*)lv_event_get_current_target(e);
  draw::logo(lv_event_get_layer(e), 233, 200, 250, lv_obj_get_style_opa(o, LV_PART_MAIN));
}
}  // namespace

void SplashView::create(lv_obj_t* parent, const char* version) {
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(root_, drawCb, LV_EVENT_DRAW_MAIN, nullptr);

  lv_obj_t* name = lv_label_create(root_);
  lv_label_set_text(name, "deskbuddy.dev");
  lv_obj_set_style_text_font(name, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(name, lv_color_hex(0xE6F0F2), 0);
  lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -92);

  lv_obj_t* ver = lv_label_create(root_);
  lv_label_set_text(ver, version);
  lv_obj_set_style_text_font(ver, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ver, lv_color_hex(0x8A989C), 0);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -68);
}

void SplashView::show(uint32_t ms) {
  total_ = ms;
  started_ = false;
  lv_obj_set_style_opa(root_, LV_OPA_COVER, 0);
  lv_obj_move_foreground(root_);
}

void SplashView::update() {
  if (!root_) return;
  // By the clock instead of summed dt: during startup (Wi-Fi, BLE) the loop still runs irregularly. The
  // clock starts at the first update, otherwise the screen is over before it was drawn even once.
  if (!started_) {
    started_ = true;
    start_ = lv_tick_get();
  }
  uint32_t t = lv_tick_elaps(start_);
  if (t >= total_) {
    // Gone is gone: delete instead of hide, and redraw the whole screen. That way nothing of the logo
    // can linger (the black disc over the eyes) and the memory is freed.
    lv_obj_t* scr = lv_obj_get_parent(root_);
    lv_obj_delete(root_);
    root_ = nullptr;
    lv_obj_invalidate(scr);
    return;
  }
  // Last 300 ms: fade out
  uint32_t left = total_ - t;
  lv_obj_set_style_opa(root_, left < 300 ? (lv_opa_t)(left * 255 / 300) : LV_OPA_COVER, 0);
}

}  // namespace buddy
