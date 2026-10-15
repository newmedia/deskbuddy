#pragma once
// Kleine tekenhulpjes bovenop de LVGL-draw-API (alleen intern voor de buddy-lib).

#include <lvgl.h>
#include <math.h>

namespace buddy {
namespace draw {

inline int32_t ri(float v) { return (int32_t)lroundf(v); }

inline void fillRect(lv_layer_t* layer, float x1, float y1, float x2, float y2, float r, lv_color_t c,
              lv_opa_t opa = LV_OPA_COVER) {
  if (x2 <= x1 || y2 <= y1 || opa == 0) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.bg_opa = opa;
  d.radius = ri(r);
  lv_area_t a = {ri(x1), ri(y1), ri(x2) - 1, ri(y2) - 1};
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
  d.p[0] = {x0, y0};
  d.p[1] = {x1, y1};
  d.p[2] = {x2, y2};
  d.color = c;
  d.opa = LV_OPA_COVER;
  lv_draw_triangle(layer, &d);
}

inline void line(lv_layer_t* layer, float x1, float y1, float x2, float y2, float width, lv_color_t c,
                 lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1 = {x1, y1};
  d.p2 = {x2, y2};
  d.color = c;
  d.width = ri(width);
  d.opa = opa;
  d.round_start = d.round_end = 1;
  lv_draw_line(layer, &d);
}

inline void frame(lv_layer_t* layer, float x1, float y1, float x2, float y2, float r, float bw, lv_color_t c,
                  lv_color_t fill, lv_opa_t fillOpa) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = fill;
  d.bg_opa = fillOpa;
  d.border_color = c;
  d.border_width = ri(bw);
  d.border_opa = LV_OPA_COVER;
  d.radius = ri(r);
  lv_area_t a = {ri(x1), ri(y1), ri(x2) - 1, ri(y2) - 1};
  lv_draw_rect(layer, &d, &a);
}

}  // namespace draw
}  // namespace buddy
