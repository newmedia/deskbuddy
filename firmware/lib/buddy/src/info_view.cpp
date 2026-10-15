#include "info_view.h"

#include <string.h>

#include "draw_util.h"

namespace buddy {

namespace {
const lv_color_t kAccent = LV_COLOR_MAKE(0x6E, 0xE7, 0xF7);
const lv_color_t kText = LV_COLOR_MAKE(0xE6, 0xF0, 0xF2);
const lv_color_t kDim = LV_COLOR_MAKE(0x8A, 0x98, 0x9C);
}  // namespace

bool DeviceInfo::operator==(const DeviceInfo& o) const {
  return wifi == o.wifi && !strcmp(ssid, o.ssid) && !strcmp(ip, o.ip) && rssi / 5 == o.rssi / 5 &&
         !strcmp(hostname, o.hostname) && bleEnabled == o.bleEnabled && bleConnected == o.bleConnected &&
         !strcmp(bleName, o.bleName) && sound == o.sound && mascot == o.mascot && batteryPct == o.batteryPct && charging == o.charging &&
         !strcmp(version, o.version);
}

lv_obj_t* InfoView::row(const char* symbol, lv_obj_t** value, lv_obj_t** sub, lv_obj_t** iconOut) {
  lv_obj_t* r = lv_obj_create(root_);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, 330, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, 16, 0);

  lv_obj_t* icon;
  if (symbol) {
    icon = lv_label_create(r);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, kAccent, 0);
    lv_obj_set_width(icon, 36);
  } else {
    icon = lv_obj_create(r);  // leeg vlak; wordt zelf getekend
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, 36, 36);
  }
  if (iconOut) *iconOut = icon;

  lv_obj_t* col = lv_obj_create(r);
  lv_obj_remove_style_all(col);
  lv_obj_set_size(col, 270, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

  *value = lv_label_create(col);
  lv_obj_set_style_text_font(*value, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(*value, kText, 0);
  lv_label_set_long_mode(*value, LV_LABEL_LONG_DOT);
  lv_obj_set_width(*value, 270);

  *sub = lv_label_create(col);
  lv_obj_set_style_text_font(*sub, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(*sub, kDim, 0);
  lv_label_set_long_mode(*sub, LV_LABEL_LONG_DOT);
  lv_obj_set_width(*sub, 270);
  return r;
}

void InfoView::create(lv_obj_t* parent) {
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(root_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(root_, 14, 0);
  lv_obj_set_style_pad_left(root_, 40, 0);

  wifiRow_ = row(LV_SYMBOL_WIFI, &wifi_, &wifiSub_);
  row(LV_SYMBOL_BLUETOOTH, &ble_, &bleSub_);
  soundRow_ = row(LV_SYMBOL_VOLUME_MAX, &sound_, &soundSub_, &soundIcon_);
  skinRow_ = row(nullptr, &skin_, &skinSub_, &skinIcon_);
  lv_obj_add_event_cb(skinIcon_, drawSkinIcon, LV_EVENT_DRAW_MAIN, this);

  footer_ = lv_label_create(root_);
  lv_obj_set_style_text_font(footer_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(footer_, kDim, 0);
  lv_obj_add_flag(footer_, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(footer_, LV_ALIGN_BOTTOM_MID, -20, -36);  // vlak boven de accuregel

  battery_ = lv_label_create(root_);
  lv_obj_set_style_text_font(battery_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(battery_, kText, 0);
  lv_obj_add_flag(battery_, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_align(battery_, LV_ALIGN_BOTTOM_MID, -20, -10);  // -20: compenseert pad_left van root

  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void InfoView::show(uint32_t autoHideMs) {
  hideIn_ = autoHideMs;
  if (visible_) return;
  visible_ = true;
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_fade_in(root_, 250, 0);
}

void InfoView::hide() {
  if (!visible_) return;
  visible_ = false;
  lv_obj_fade_out(root_, 250, 0);
}

void InfoView::drawSkinIcon(lv_event_t* e) {
  using namespace draw;
  auto* self = static_cast<InfoView*>(lv_event_get_user_data(e));
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t a;
  lv_obj_get_coords(self->skinIcon_, &a);
  float cx = (a.x1 + a.x2 + 1) / 2.0f, cy = (a.y1 + a.y2 + 1) / 2.0f;

  if (self->last_.mascot) {
    // Huidkleurig kopje met blauw haar, brilletje en lach
    fillEllipse(layer, cx, cy, 36, 36, lv_color_hex(0xF5C99B));
    lv_area_t saved = layer->_clip_area;
    lv_area_t top = saved;
    top.y2 = LV_MIN(top.y2, ri(cy - 5));
    layer->_clip_area = top;
    fillEllipse(layer, cx, cy, 36, 36, lv_color_hex(0x0B2290));
    layer->_clip_area = saved;
    fillTri(layer, cx - 12, cy - 7, cx - 4, cy - 7, cx - 11, cy - 1, lv_color_hex(0x0B2290));
    fillTri(layer, cx, cy - 7, cx + 8, cy - 7, cx + 2, cy - 1, lv_color_hex(0x0B2290));
    frame(layer, cx - 13, cy - 2, cx - 2, cy + 6, 2, 2, lv_color_hex(0x15171C), lv_color_white(), LV_OPA_COVER);
    frame(layer, cx + 2, cy - 2, cx + 13, cy + 6, 2, 2, lv_color_hex(0x15171C), lv_color_white(), LV_OPA_COVER);
    fillEllipse(layer, cx - 7.5f, cy + 2, 4, 4, lv_color_hex(0x22378F));
    fillEllipse(layer, cx + 7.5f, cy + 2, 4, 4, lv_color_hex(0x22378F));
    lv_area_t bottom = saved;
    bottom.y1 = LV_MAX(bottom.y1, ri(cy + 9));
    layer->_clip_area = bottom;
    fillEllipse(layer, cx, cy + 9, 14, 12, lv_color_hex(0x5A0F1C));
    layer->_clip_area = saved;
  } else {
    // Zwart rondje met rand en twee cyaan robotoogjes
    fillEllipse(layer, cx, cy, 36, 36, kAccent);
    fillEllipse(layer, cx, cy, 31, 31, lv_color_black());
    fillRect(layer, cx - 10, cy - 7, cx - 3, cy + 5, 3, kAccent);
    fillRect(layer, cx + 3, cy - 7, cx + 10, cy + 5, 3, kAccent);
  }
}

InfoView::Hit InfoView::hit(int x, int y) const {
  if (!visible_) return Hit::None;
  const int m = 7;  // halve rij-afstand als marge: kleine vinger, klein scherm
  auto inside = [&](lv_obj_t* o) {
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    return x >= a.x1 - m && x <= a.x2 + m && y >= a.y1 - m && y <= a.y2 + m;
  };
  if (inside(wifiRow_)) return Hit::Wifi;
  if (inside(soundRow_)) return Hit::Sound;
  if (inside(skinRow_)) return Hit::Skin;
  return Hit::None;
}

void InfoView::update(const DeviceInfo& in, uint32_t dtMs) {
  if (visible_ && hideIn_) {
    if (dtMs >= hideIn_) hide();
    else hideIn_ -= dtMs;
  }
  if (!visible_ && lv_obj_get_style_opa(root_, LV_PART_MAIN) == LV_OPA_TRANSP) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
  if (!first_ && in == last_) return;
  first_ = false;
  last_ = in;

  using M = DeviceInfo::WifiMode;
  switch (in.wifi) {
    case M::Station:
      lv_label_set_text(wifi_, in.ip);
      lv_label_set_text_fmt(wifiSub_, "%s  %d dBm  %s.local", in.ssid, in.rssi, in.hostname);
      break;
    case M::AccessPoint:
      lv_label_set_text(wifi_, in.ip);
      lv_label_set_text_fmt(wifiSub_, "Setup: %s (tik voor QR)", in.ssid);
      break;
    case M::Connecting:
      lv_label_set_text(wifi_, "Verbinden...");
      lv_label_set_text(wifiSub_, in.ssid);
      break;
    default:
      lv_label_set_text(wifi_, "Niet verbonden");
      lv_label_set_text(wifiSub_, "Tik om Wi-Fi in te stellen");
      break;
  }

  if (!in.bleEnabled) {
    lv_label_set_text(ble_, "Uit");
    lv_label_set_text(bleSub_, "Bluetooth uitgeschakeld");
  } else {
    lv_label_set_text(ble_, in.bleName);
    lv_label_set_text(bleSub_, in.bleConnected ? "Verbonden" : "Zichtbaar");
  }

  lv_label_set_text(soundIcon_, in.sound ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);
  lv_label_set_text(sound_, in.sound ? "Geluid aan" : "Geluid uit");
  lv_label_set_text(soundSub_, "Tik om te wisselen");
  lv_label_set_text(skin_, in.mascot ? "Mascotte" : "Robot");
  lv_obj_invalidate(skinIcon_);
  lv_label_set_text(skinSub_, "Tik voor ander uiterlijk");

  lv_label_set_text_fmt(footer_, "deskbuddy %s", in.version);
  if (in.batteryPct < 0)
    lv_label_set_text(battery_, LV_SYMBOL_USB "  USB");
  else
    lv_label_set_text_fmt(battery_, "%s  %d%%", in.charging ? LV_SYMBOL_CHARGE : LV_SYMBOL_BATTERY_FULL, in.batteryPct);
}

}  // namespace buddy
