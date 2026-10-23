#include "setup_view.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"

namespace buddy {

void SetupView::create(lv_obj_t* parent) {
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  title_ = lv_label_create(root_);
  lv_obj_set_style_text_font(title_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(title_, lv_color_hex(0x6EE7F7), 0);
  lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 52);

  qr_ = lv_qrcode_create(root_);
  lv_qrcode_set_size(qr_, 190);
  lv_qrcode_set_dark_color(qr_, lv_color_black());
  lv_qrcode_set_light_color(qr_, lv_color_white());
  lv_obj_set_style_border_color(qr_, lv_color_white(), 0);
  lv_obj_set_style_border_width(qr_, 8, 0);
  lv_obj_align(qr_, LV_ALIGN_CENTER, 0, 4);

  hint_ = lv_label_create(root_);
  lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(hint_, lv_color_hex(0x9AA8AD), 0);
  lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(hint_, 300);
  lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -48);

  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SetupView::show(const char* ssid, const char* ip, uint32_t autoHideMs) {
  // Standard Wi-Fi QR (open network): phone cameras then offer "connect" right away.
  char qr[80];
  snprintf(qr, sizeof(qr), "WIFI:T:nopass;S:%s;;", ssid);
  lv_qrcode_update(qr_, qr, (uint32_t)strlen(qr));
  snprintf(ssid_, sizeof(ssid_), "%s", ssid);
  snprintf(ip_, sizeof(ip_), "%s", ip);
  linkStep_ = false;
  retext();
  hideIn_ = autoHideMs;
  if (visible_) return;
  visible_ = true;
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_fade_in(root_, 250, 0);
}

void SetupView::showLinkStep() {
  if (linkStep_) return;
  linkStep_ = true;
  char url[40];
  snprintf(url, sizeof(url), "http://%s/", ip_);
  lv_qrcode_update(qr_, url, (uint32_t)strlen(url));
  retext();
  hideIn_ = 180000;
}

void SetupView::retext() {
  lang_ = gLang;
  if (linkStep_) {
    lv_label_set_text(title_, tr("Step 2: open the\nsetup page", "Stap 2: open de\nsetup-pagina"));
    lv_label_set_text_fmt(hint_, tr("Scan again, or go to\n%s", "Scan opnieuw, of ga naar\n%s"), ip_);
  } else {
    lv_label_set_text_fmt(title_, tr("Step 1: connect to\n%s", "Stap 1: verbind met\n%s"), ssid_);
    lv_label_set_text(hint_, tr("Scan with your phone camera", "Scan met je telefooncamera"));
  }
}

void SetupView::hide() {
  if (!visible_) return;
  visible_ = false;
  lv_obj_fade_out(root_, 250, 0);
}

void SetupView::update(uint32_t dtMs) {
  if (visible_ && lang_ != gLang) retext();
  if (visible_ && hideIn_) {
    if (dtMs >= hideIn_) hide();
    else hideIn_ -= dtMs;
  }
  if (!visible_ && lv_obj_get_style_opa(root_, LV_PART_MAIN) == LV_OPA_TRANSP) lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PairView::create(lv_obj_t* parent) {
  root_ = lv_obj_create(parent);
  lv_obj_remove_style_all(root_);
  lv_obj_set_size(root_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(root_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* icon = lv_label_create(root_);
  lv_label_set_text(icon, LV_SYMBOL_BLUETOOTH);
  lv_obj_set_style_text_font(icon, &buddy_48, 0);
  lv_obj_set_style_text_color(icon, lv_color_hex(0x6EE7F7), 0);
  lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 70);

  title_ = lv_label_create(root_);
  lv_obj_set_style_text_font(title_, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(title_, lv_color_hex(0xE6F0F2), 0);
  lv_obj_align(title_, LV_ALIGN_CENTER, 0, -55);

  code_ = lv_label_create(root_);
  lv_obj_set_style_text_font(code_, &buddy_48, 0);
  lv_obj_set_style_text_color(code_, lv_color_hex(0x6EE7F7), 0);
  lv_obj_set_style_text_letter_space(code_, 6, 0);
  lv_obj_align(code_, LV_ALIGN_CENTER, 0, 10);

  hint_ = lv_label_create(root_);
  lv_obj_set_style_text_font(hint_, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(hint_, lv_color_hex(0x9AA8AD), 0);
  lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(hint_, LV_ALIGN_CENTER, 0, 90);

  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PairView::show(uint32_t code) {
  lv_label_set_text(title_, tr("Pairing code", "Koppelcode"));
  lv_label_set_text(hint_, tr("Enter this code\non your phone", "Vul deze code in\nop je telefoon"));
  lv_label_set_text_fmt(code_, "%03lu %03lu", (unsigned long)(code / 1000), (unsigned long)(code % 1000));
  if (visible_) return;
  visible_ = true;
  lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PairView::hide() {
  if (!visible_) return;
  visible_ = false;
  lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace buddy
