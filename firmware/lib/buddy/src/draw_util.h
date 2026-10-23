#pragma once
// Small drawing helpers on top of the LVGL draw API (internal to the buddy lib only).

#include <lvgl.h>
#include <math.h>

namespace buddy {
namespace draw {

inline int32_t ri(float v) { return (int32_t)lroundf(v); }

// Offset of everything that is drawn (bouncing, shaking). The face draws in fixed
// screen coordinates; this way the content shifts without moving the object.
inline float ox = 0, oy = 0;
inline void shift(lv_area_t& a) { lv_area_move(&a, ri(ox), ri(oy)); }
inline void shift(lv_point_t& p) { p.x += ri(ox); p.y += ri(oy); }
struct NoShift {  // temporarily without offset (background, effects over the whole screen)
  float sx = ox, sy = oy;
  NoShift() { ox = oy = 0; }
  ~NoShift() { ox = sx; oy = sy; }
};

inline void fillRect(lv_layer_t* layer, float x1, float y1, float x2, float y2, float r, lv_color_t c,
              lv_opa_t opa = LV_OPA_COVER) {
  if (x2 <= x1 || y2 <= y1 || opa == 0) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.bg_opa = opa;
  d.radius = ri(r);
  lv_area_t a = {ri(x1 + ox), ri(y1 + oy), ri(x2 + ox) - 1, ri(y2 + oy) - 1};
  lv_draw_rect(layer, &d, &a);
}

inline void fillEllipse(lv_layer_t* layer, float cx, float cy, float w, float h, lv_color_t c,
                 lv_opa_t opa = LV_OPA_COVER) {
  fillRect(layer, cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, LV_RADIUS_CIRCLE, c, opa);
}

inline void fillTri(lv_layer_t* layer, float x0, float y0, float x1, float y1, float x2, float y2,
             lv_color_t c) {
  lv_draw_triangle_dsc_t d;
  lv_draw_triangle_dsc_init(&d);
  d.p[0] = {x0 + ox, y0 + oy};
  d.p[1] = {x1 + ox, y1 + oy};
  d.p[2] = {x2 + ox, y2 + oy};
  d.color = c;
  d.opa = LV_OPA_COVER;
  lv_draw_triangle(layer, &d);
}

inline void line(lv_layer_t* layer, float x1, float y1, float x2, float y2, float width, lv_color_t c,
                 lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1 = {x1 + ox, y1 + oy};
  d.p2 = {x2 + ox, y2 + oy};
  d.color = c;
  d.width = ri(width);
  d.opa = opa;
  d.round_start = d.round_end = 1;
  lv_draw_line(layer, &d);
}

inline void frame(lv_layer_t* layer, float x1, float y1, float x2, float y2, float r, float bw, lv_color_t c,
                  lv_color_t fill, lv_opa_t fillOpa, lv_opa_t borderOpa = LV_OPA_COVER) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = fill;
  d.bg_opa = fillOpa;
  d.border_color = c;
  d.border_width = ri(bw);
  d.border_opa = borderOpa;
  d.radius = ri(r);
  lv_area_t a = {ri(x1 + ox), ri(y1 + oy), ri(x2 + ox) - 1, ri(y2 + oy) - 1};
  lv_draw_rect(layer, &d, &a);
}

// The deskbuddy logo (same shapes as firmware/web/icon.svg): cyan ring, two eyes and a smile.
// size = diameter of the ring in pixels; cx/cy = centre. Draws unshifted (independent of the face).
// opa: pass it yourself for a fading object; custom drawing ignores LVGL's opa style, so
// otherwise an opaque black circle stays behind while the rest is already gone.
inline void logo(lv_layer_t* layer, float cx, float cy, float size, lv_opa_t opa = LV_OPA_COVER,
                 lv_color_t col = lv_color_hex(0x6EE7F7)) {
  if (opa <= LV_OPA_MIN) return;
  const float k = size / 400.0f;  // ring: radius 200 (+9 line) in the 512 space of the svg
  const float ox0 = ox, oy0 = oy;
  ox = oy = 0;
  frame(layer, cx - 200 * k, cy - 200 * k, cx + 200 * k, cy + 200 * k, LV_RADIUS_CIRCLE, LV_MAX(2.0f, 18 * k), col,
        lv_color_black(), opa, opa);
  fillRect(layer, cx - 106 * k, cy - 86 * k, cx - 28 * k, cy + 34 * k, 30 * k, col, opa);
  fillRect(layer, cx + 28 * k, cy - 86 * k, cx + 106 * k, cy + 34 * k, 30 * k, col, opa);
  lv_draw_arc_dsc_t a;
  lv_draw_arc_dsc_init(&a);
  a.center = {ri(cx), ri(cy + 24 * k)};  // "q50 40 100 0" in the svg: a shallow arc below the eyes
  a.radius = (uint16_t)LV_MAX(3.0f, 90 * k);
  a.width = (uint16_t)LV_MAX(2.0f, 16 * k);
  a.rounded = 1;
  a.color = col;
  a.opa = opa;
  a.start_angle = 45;
  a.end_angle = 135;
  lv_draw_arc(layer, &a);
  ox = ox0;
  oy = oy0;
}

inline void heart(lv_layer_t* layer, float cx, float cy, float s, lv_color_t c) {
  float r = s * 0.27f;
  fillEllipse(layer, cx - s * 0.23f, cy - s * 0.12f, 2 * r, 2 * r, c);
  fillEllipse(layer, cx + s * 0.23f, cy - s * 0.12f, 2 * r, 2 * r, c);
  fillTri(layer, cx - s * 0.49f, cy - s * 0.06f, cx + s * 0.49f, cy - s * 0.06f, cx, cy + s * 0.44f, c);
}

}  // namespace draw
}  // namespace buddy
