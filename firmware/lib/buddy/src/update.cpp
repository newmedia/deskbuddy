#include "update.h"

#include <stdlib.h>
#include <string.h>

namespace buddy {

namespace {
const lv_color_t kAccent = LV_COLOR_MAKE(0x6E, 0xE7, 0xF7);
const lv_color_t kTrack = LV_COLOR_MAKE(0x16, 0x26, 0x2B);
const lv_color_t kText = LV_COLOR_MAKE(0xE6, 0xF0, 0xF2);

void parse(const char* s, long v[3]) {
  v[0] = v[1] = v[2] = 0;
  if (!s) return;
  if (*s == 'v' || *s == 'V') s++;
  for (int i = 0; i < 3 && *s >= '0' && *s <= '9'; i++) {
    char* end;
    v[i] = strtol(s, &end, 10);
    s = end;
    if (*s != '.') break;
    s++;
  }
}
}  // namespace

const char* updateModeName(UpdateMode m) {
  switch (m) {
    case UpdateMode::Off: return "off";
    case UpdateMode::Auto: return "auto";
    default: return "notify";
  }
}

bool updateModeFromName(const char* n, UpdateMode& out) {
  if (!strcmp(n, "off")) out = UpdateMode::Off;
  else if (!strcmp(n, "notify")) out = UpdateMode::Notify;
  else if (!strcmp(n, "auto")) out = UpdateMode::Auto;
  else return false;
  return true;
}

const char* updateStateName(UpdateState s) {
  switch (s) {
    case UpdateState::Checking: return "checking";
    case UpdateState::UpToDate: return "uptodate";
    case UpdateState::Available: return "available";
    case UpdateState::Installing: return "installing";
    case UpdateState::Failed: return "failed";
    default: return "idle";
  }
}

int compareVersions(const char* a, const char* b) {
  long x[3], y[3];
  parse(a, x);
  parse(b, y);
  for (int i = 0; i < 3; i++)
    if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}

void UpdateView::create(lv_obj_t* parent) {
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  arc_ = lv_arc_create(root_);
  lv_obj_set_size(arc_, LV_PCT(100), LV_PCT(100));
  lv_obj_center(arc_);
  lv_arc_set_rotation(arc_, 270);
  lv_arc_set_bg_angles(arc_, 0, 360);
  lv_arc_set_range(arc_, 0, 100);
  lv_arc_set_value(arc_, 0);
  lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(arc_, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc_, kTrack, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc_, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc_, kAccent, LV_PART_INDICATOR);

  label_ = lv_label_create(root_);
  lv_obj_set_style_text_font(label_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label_, kText, 0);
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label_, LV_ALIGN_BOTTOM_MID, 0, -48);

  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void UpdateView::update(bool installing, uint8_t pct, const char* version) {
  if (installing != visible_) {
    visible_ = installing;
    lastPct_ = -1;
    if (installing) {
      lv_obj_move_foreground(root_);
      lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (!visible_ || pct == lastPct_) return;
  lastPct_ = pct;
  lv_arc_set_value(arc_, pct);
  lv_label_set_text_fmt(label_, "Bijwerken naar %s\n%u%%", version, (unsigned)pct);
}

}  // namespace buddy
