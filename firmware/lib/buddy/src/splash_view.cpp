#include "splash_view.h"

#include "draw_util.h"

namespace buddy {

namespace {
void drawCb(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  draw::logo(layer, 233, 200, 250);
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
  lv_label_set_text(name, "deskbuddy");
  lv_obj_set_style_text_font(name, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(name, lv_color_hex(0xE6F0F2), 0);
  lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -92);

  lv_obj_t* ver = lv_label_create(root_);
  lv_label_set_text(ver, version);
  lv_obj_set_style_text_font(ver, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(ver, lv_color_hex(0x8A989C), 0);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_MID, 0, -68);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SplashView::show(uint32_t ms) {
  left_ = total_ = ms;
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(root_);
}

void SplashView::update(uint32_t dtMs) {
  if (!left_) return;
  left_ = dtMs >= left_ ? 0 : left_ - dtMs;
  if (!left_) {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  // Laatste 300 ms: wegfaden
  lv_obj_set_style_opa(root_, left_ < 300 ? (lv_opa_t)(left_ * 255 / 300) : LV_OPA_COVER, 0);
}

}  // namespace buddy
