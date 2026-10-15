#include "clock_view.h"

#include <stdio.h>

namespace buddy {

namespace {
const char* const kDays[] = {"zondag", "maandag", "dinsdag", "woensdag", "donderdag", "vrijdag", "zaterdag"};
const char* const kMonths[] = {"jan", "feb", "mrt", "apr", "mei", "jun", "jul", "aug", "sep", "okt", "nov", "dec"};
}  // namespace

void ClockView::create(lv_obj_t* parent) {
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  batt_ = lv_arc_create(root_);
  lv_obj_set_size(batt_, 446, 446);
  lv_obj_center(batt_);
  lv_arc_set_bg_angles(batt_, 135, 45);
  lv_arc_set_range(batt_, 0, 100);
  lv_obj_remove_style(batt_, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(batt_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(batt_, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(batt_, 8, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(batt_, lv_color_hex(0x1A2A2E), LV_PART_MAIN);

  time_ = lv_label_create(root_);
  lv_obj_set_style_text_font(time_, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(time_, lv_color_hex(0x6EE7F7), 0);
  lv_obj_set_style_transform_scale(time_, 512, 0);  // 2x: groot leesbaar op 466px
  lv_obj_set_style_transform_pivot_x(time_, LV_PCT(50), 0);
  lv_obj_set_style_transform_pivot_y(time_, LV_PCT(50), 0);
  lv_obj_align(time_, LV_ALIGN_CENTER, 0, -20);

  date_ = lv_label_create(root_);
  lv_obj_set_style_text_font(date_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(date_, lv_color_hex(0x9AA8AD), 0);
  lv_obj_align(date_, LV_ALIGN_CENTER, 0, 70);

  battLabel_ = lv_label_create(root_);
  lv_obj_set_style_text_font(battLabel_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(battLabel_, lv_color_hex(0x9AA8AD), 0);
  lv_obj_align(battLabel_, LV_ALIGN_BOTTOM_MID, 0, -40);

  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ClockView::show(uint32_t autoHideMs) {
  hideIn_ = autoHideMs;
  if (visible_) return;
  visible_ = true;
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_fade_in(root_, 250, 0);
}

void ClockView::hide() {
  if (!visible_) return;
  visible_ = false;
  lv_obj_fade_out(root_, 250, 0);
}

void ClockView::update(const struct tm& now, int pct, bool charging, uint32_t dtMs) {
  if (visible_ && hideIn_) {
    if (dtMs >= hideIn_) hide();
    else hideIn_ -= dtMs;
  }
  if (!visible_ && lv_obj_get_style_opa(root_, LV_PART_MAIN) == LV_OPA_TRANSP) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

  if (now.tm_min != lastMin_) {
    lastMin_ = now.tm_min;
    lv_label_set_text_fmt(time_, "%02d:%02d", now.tm_hour, now.tm_min);
    lv_label_set_text_fmt(date_, "%s %d %s", kDays[now.tm_wday % 7], now.tm_mday, kMonths[now.tm_mon % 12]);
  }
  if (pct != lastPct_ || charging != lastCharging_) {
    lastPct_ = pct;
    lastCharging_ = charging;
    if (pct < 0) {
      lv_arc_set_value(batt_, 100);
      lv_obj_set_style_arc_color(batt_, lv_color_hex(0x2E7F8C), LV_PART_INDICATOR);
      lv_label_set_text(battLabel_, LV_SYMBOL_USB);
    } else {
      lv_arc_set_value(batt_, pct);
      uint32_t c = charging ? 0x6EE7F7 : (pct > 40 ? 0x5BE37D : (pct > 15 ? 0xFFB347 : 0xFF5A4E));
      lv_obj_set_style_arc_color(batt_, lv_color_hex(c), LV_PART_INDICATOR);
      lv_label_set_text_fmt(battLabel_, "%s %d%%", charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL, pct);
    }
  }
}

}  // namespace buddy
