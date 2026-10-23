#include "update.h"

#include <stdlib.h>
#include <string.h>

#include "i18n.h"

namespace buddy {

namespace {
const lv_color_t kAccent = LV_COLOR_MAKE(0x00, 0xFF, 0x66);  // matrix green
const lv_color_t kTrack = LV_COLOR_MAKE(0x08, 0x2A, 0x14);
const lv_color_t kText = LV_COLOR_MAKE(0xB8, 0xFF, 0xD0);

// "v1.4.0-beta.1" -> {1, 4, 0}; returns the pre-release part ("beta.1") or "" for a normal version
const char* parse(const char* s, long v[3]) {
  v[0] = v[1] = v[2] = 0;
  if (!s) return "";
  if (*s == 'v' || *s == 'V') s++;
  for (int i = 0; i < 3 && *s >= '0' && *s <= '9'; i++) {
    char* end;
    v[i] = strtol(s, &end, 10);
    s = end;
    if (*s != '.') break;
    s++;
  }
  return *s == '-' ? s + 1 : "";
}

// Pre-release parts per semver: dot-separated identifiers, numbers compared as numbers
int comparePre(const char* a, const char* b) {
  while (*a && *b) {
    size_t la = strcspn(a, "."), lb = strcspn(b, ".");
    bool na = la && strspn(a, "0123456789") >= la, nb = lb && strspn(b, "0123456789") >= lb;
    int c;
    if (na && nb) {
      long x = strtol(a, nullptr, 10), y = strtol(b, nullptr, 10);
      c = x < y ? -1 : x > y;
    } else if (na != nb) {
      c = na ? -1 : 1;  // numbers sort before words
    } else {
      c = strncmp(a, b, la < lb ? la : lb);
      if (!c) c = la < lb ? -1 : la > lb;
    }
    if (c) return c < 0 ? -1 : 1;
    a += la + (a[la] == '.');
    b += lb + (b[lb] == '.');
  }
  return *a ? 1 : *b ? -1 : 0;
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
  const char* pa = parse(a, x);
  const char* pb = parse(b, y);
  for (int i = 0; i < 3; i++)
    if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
  // Same numbers: a pre-release comes before the release itself (1.4.0-beta.1 < 1.4.0-rc.1 < 1.4.0)
  if (!*pa || !*pb) return *pa ? -1 : *pb ? 1 : 0;
  return comparePre(pa, pb);
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
  lv_label_set_text_fmt(label_, tr("Updating to %s\n%u%%", "Bijwerken naar %s\n%u%%"), version, (unsigned)pct);
}

}  // namespace buddy
