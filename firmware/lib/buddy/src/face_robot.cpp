// Robot-basis met bouwdoos-onderdelen: ogen (blok, rond, pil, ring, pixel, vizier), wenkbrauwen,
// mond (auto, lijn, zigzag, LED-equalizer, pixel), wangen en bril, in eigen kleuren.
// Emoties sturen dezelfde veren als altijd; elk onderdeel tekent ze op zijn eigen manier.

#include <math.h>

#include "draw_util.h"
#include "face.h"

namespace buddy {

using namespace draw;

namespace {

constexpr float C = Face::kSize / 2.0f;
constexpr float kEyeSpacing = 94, kEyeY = -18;
constexpr float kLookX = 58, kLookY = 42;
constexpr float kPi = 3.14159265f;
const uint32_t kBlushColor = 0xFF6FA8;

inline lv_color_t hex(uint32_t c) { return lv_color_hex(c); }

// Ligt (px,py) binnen een afgeronde rechthoek?
bool inRoundRect(float px, float py, float x1, float y1, float x2, float y2, float r) {
  if (px < x1 || px > x2 || py < y1 || py > y2) return false;
  float cx = LV_CLAMP(x1 + r, px, x2 - r), cy = LV_CLAMP(y1 + r, py, y2 - r);
  float dx = px - cx, dy = py - cy;
  return dx * dx + dy * dy <= r * r;
}

bool inHeart(float px, float py, float cx, float cy, float s, float vs) {
  float r = s * 0.27f;
  float ly = cy - s * 0.12f * vs;
  auto inCircle = [&](float ox) {
    float dx = px - ox, dy = (py - ly) / vs;
    return dx * dx + dy * dy <= r * r;
  };
  if (inCircle(cx - s * 0.23f) || inCircle(cx + s * 0.23f)) return true;
  // driehoek: van (cx±0.49s, cy-0.06s) naar (cx, cy+0.44s)
  float top = cy - s * 0.06f * vs, bot = cy + s * 0.44f * vs;
  if (py < top || py > bot) return false;
  float half = s * 0.49f * (1 - (py - top) / (bot - top));
  return fabsf(px - cx) <= half;
}

}  // namespace

// LED-raster: vierkante "pixels" binnen de vorm (afgeronde rechthoek of hart).
void Face::drawPixelShape(lv_layer_t* layer, float x1, float y1, float x2, float y2, float r, bool heart,
                          lv_color_t col) {
  const float cell = 14, dot = 11;
  float cx = (x1 + x2) / 2, cy = (y1 + y2) / 2;
  float s = (x2 - x1) * 1.08f, vs = 1;
  // raster vast t.o.v. het oogmidden, zodat pixels niet "zwemmen" bij kleine bewegingen
  for (float y = cy - floorf((cy - y1) / cell) * cell; y <= y2; y += cell)
    for (float x = cx - floorf((cx - x1) / cell) * cell; x <= x2; x += cell) {
      bool in = heart ? inHeart(x, y, cx, cy, s, vs) : inRoundRect(x, y, x1, y1, x2, y2, r);
      if (in) fillRect(layer, x - dot / 2, y - dot / 2, x + dot / 2, y + dot / 2, 2, col);
    }
}

void Face::drawRobot(lv_layer_t* layer) {
  float bob = (float)bobPx_;
  drawnBobPx_ = bobPx_;
  const lv_color_t bg = hex(look_.face);
  const lv_color_t col = lv_color_make((uint8_t)LV_CLAMP(0.f, colR_.x, 255.f), (uint8_t)LV_CLAMP(0.f, colG_.x, 255.f),
                                       (uint8_t)LV_CLAMP(0.f, colB_.x, 255.f));
  if (look_.face != 0) fillRect(layer, 0, 0, kSize, kSize, 0, bg);  // eigen achtergrondkleur
  float baseY = C + kEyeY + dy_.x + lookY_.x + bob;

  // Vizier: één brede balk over beide ogen (de oogleden per oog vormen hem daarna per emotie)
  const EyeStyle es = (EyeStyle)look_.eyes;
  if (es == EyeStyle::Visor && !heartShown_) {
    float c = LV_MAX(eyeClosure(-1), eyeClosure(1));
    float h = LV_MAX(eye_.h.x * 0.5f * swell_ * (1 - c * 0.92f), 6.0f);
    float x1 = C - kEyeSpacing - eye_.w.x / 2 + lookX_.x * 0.5f, x2 = C + kEyeSpacing + eye_.w.x / 2 + lookX_.x * 0.5f;
    fillRect(layer, x1, baseY - h / 2, x2, baseY + h / 2, LV_RADIUS_CIRCLE, col);
    // "scanner": een lichter vlekje dat de kijkrichting volgt
    float sx = C + lookX_.x * 2.2f;
    fillEllipse(layer, sx, baseY, 56, h * 0.7f, lv_color_mix(lv_color_white(), col, 150), 220);
  }

  for (int side = -1; side <= 1; side += 2) {
    float persp = 1.0f + side * (lookX_.x / kLookX) * 0.07f;  // oog aan de kijkkant iets groter
    float cx = C + side * kEyeSpacing + lookX_.x;
    float cy = baseY;
    if (emotion_ == Emotion::Dizzy) {
      float a = t_ * 7.0f * side;
      cx += cosf(a) * 16;
      cy += sinf(a) * 16;
    }
    drawEye(layer, side, cx, cy, persp);
  }

  drawRobotBrows(layer, baseY);
  drawRobotGlasses(layer, baseY);

  float mx = C + lookX_.x * 0.4f, my = C + bob;
  if (pucker_)
    drawPucker(layer, mx, my + 112, col, false);
  else if ((MouthStyle)look_.mouth != MouthStyle::Auto || (mouthShown_ != MouthNone && mouthAlpha_ > 0.01f))
    drawMouth(layer, mx, my);
  if (emotion_ == Emotion::Sleepy || emotion_ == Emotion::Asleep) drawZs(layer, col);
}

void Face::drawEye(lv_layer_t* layer, int side, float cx, float cy, float scale) {
  const lv_color_t col = lv_color_make((uint8_t)LV_CLAMP(0.f, colR_.x, 255.f), (uint8_t)LV_CLAMP(0.f, colG_.x, 255.f),
                                       (uint8_t)LV_CLAMP(0.f, colB_.x, 255.f));
  const lv_color_t bg = hex(look_.face);
  const EyeStyle es = (EyeStyle)look_.eyes;
  float sq = LV_CLAMP(-0.6f, squash_.x, 0.6f) * 0.25f;
  float pulse = (emotion_ == Emotion::Love) ? 1.0f + 0.06f * sinf(t_ * 9.0f) : 1.0f;
  float w = eye_.w.x * scale * (1 + sq) * pulse * swell_;
  float h = eye_.h.x * scale * (1 - sq) * pulse * swell_;
  if (es == EyeStyle::Round) w = h = LV_MIN(w, h) * 1.02f;  // rond: even breed als hoog
  if (es == EyeStyle::Pill) w *= 0.68f;
  if (es == EyeStyle::Visor) h *= 0.5f;
  float c = eyeClosure(side);
  float hv = LV_MAX(h * (1 - c * 0.94f), 8.0f);
  float vs = hv / h;
  float x1 = cx - w / 2, x2 = cx + w / 2;
  float y1 = cy - hv / 2, y2 = cy + hv / 2;

  // Wangen onder de buitenkant van het oog
  const CheekStyle cs = (CheekStyle)look_.cheeks;
  float bx = cx + side * 18, by = cy + eye_.h.x / 2 + 26;
  if (cs == CheekStyle::Blush && blush_.x > 0.02f) {
    fillEllipse(layer, bx, by, 74, 34, hex(kBlushColor), (lv_opa_t)LV_CLAMP(0.f, blush_.x * 190, 255.f));
  } else if (cs == CheekStyle::Stripes) {
    lv_opa_t o = (lv_opa_t)LV_CLAMP(0.f, 110 + blush_.x * 145, 255.f);
    for (int i = -1; i <= 1; i++) line(layer, bx + i * 16 - 6, by + 10, bx + i * 16 + 6, by - 10, 5, hex(kBlushColor), o);
  } else if (cs == CheekStyle::Pixel) {
    lv_opa_t o = (lv_opa_t)LV_CLAMP(0.f, 120 + blush_.x * 135, 255.f);
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 2; j++)
        fillRect(layer, bx - 21 + i * 14, by - 8 + j * 14, bx - 10 + i * 14, by + 3 + j * 14, 2, hex(kBlushColor), o);
  }

  if (heartShown_) {
    float s = w * 1.08f, r = s * 0.27f;
    if (es == EyeStyle::Pixel) {
      drawPixelShape(layer, cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2, 0, true, col);
    } else {
      fillEllipse(layer, cx - s * 0.23f, cy - s * 0.12f * vs, 2 * r, 2 * r * vs, col);
      fillEllipse(layer, cx + s * 0.23f, cy - s * 0.12f * vs, 2 * r, 2 * r * vs, col);
      fillTri(layer, cx - s * 0.49f, cy - s * 0.06f * vs, cx + s * 0.49f, cy - s * 0.06f * vs, cx, cy + s * 0.44f * vs,
              col);
    }
    return;
  }

  float r = LV_MIN(eye_.r.x * scale, LV_MIN(w, hv) / 2);
  if (es == EyeStyle::Round || es == EyeStyle::Pill) r = LV_RADIUS_CIRCLE;
  switch (es) {
    case EyeStyle::Visor:
      break;  // de balk is al getekend; hieronder alleen de oogleden
    case EyeStyle::Ring: {
      fillRect(layer, x1, y1, x2, y2, r, col);
      float t = LV_MIN(13.0f, LV_MIN(w, hv) / 3);
      fillRect(layer, x1 + t, y1 + t, x2 - t, y2 - t, LV_MAX(0.f, r - t), bg);
      // pupil die de kijkrichting volgt
      float pr = LV_MIN(w, hv) * 0.2f;
      float px = cx + LV_CLAMP(-1.f, lookX_.x / kLookX, 1.f) * (w / 2 - t - pr - 3);
      float py = cy + LV_CLAMP(-1.f, lookY_.x / kLookY, 1.f) * LV_MAX(0.f, hv / 2 - t - pr - 3);
      if (hv > 3 * t) fillEllipse(layer, px, py, 2 * pr, 2 * pr, col);
      break;
    }
    case EyeStyle::Pixel:
      drawPixelShape(layer, x1, y1, x2, y2, (float)LV_MIN((int32_t)r, (int32_t)(LV_MIN(w, hv) / 2)), false, col);
      break;
    default:
      fillRect(layer, x1, y1, x2, y2, r, col);
      break;
  }

  // Bovenooglid in achtergrondkleur met schuine onderrand (binnenhoek = kant richting schermmidden)
  float lid = LV_CLAMP(0.f, eye_.lidTop.x, 1.f);
  float tilt = eye_.tilt.x;
  if (lid > 0.005f || fabsf(tilt) > 0.5f) {
    float slope = tanf(tilt * kPi / 180.0f) * (w / 2 + 8);
    float base = y1 + lid * hv;
    float yInner = base + slope, yOuter = base - slope;
    float yl = (side < 0) ? yOuter : yInner, yr = (side < 0) ? yInner : yOuter;
    float top = y1 - 12;
    yl = LV_MAX(yl, top);
    yr = LV_MAX(yr, top);
    float ymin = LV_MIN(yl, yr), xl = x1 - 8, xr = x2 + 8;
    fillRect(layer, xl, top, xr, ymin + 2, 0, bg);
    if (yl < yr) fillTri(layer, xl, yl, xr, yl, xr, yr, bg);
    else if (yr < yl) fillTri(layer, xl, yr, xr, yr, xl, yl, bg);
  }
  // Onderooglid (blije "^"-vorm)
  float lidB = LV_CLAMP(0.f, eye_.lidBottom.x, 1.f);
  if (lidB > 0.005f) {
    float ew = w * 1.7f, eh = hv * 1.3f;
    fillEllipse(layer, cx, y2 - lidB * hv + eh / 2, ew, eh, bg);
  }
}

void Face::drawRobotBrows(lv_layer_t* layer, float cy) {
  const BrowStyle bs = (BrowStyle)look_.brows;
  if (bs == BrowStyle::None) return;
  const lv_color_t col = hex(look_.accent);
  float ang = (browAngle_.x + eye_.tilt.x * 0.6f) * kPi / 180.0f;
  if (bs == BrowStyle::Thick) ang *= 1.3f;
  float width = bs == BrowStyle::Thick ? 18 : (bs == BrowStyle::Thin ? 6 : 12);
  for (int side = -1; side <= 1; side += 2) {
    float bx = C + side * kEyeSpacing + lookX_.x * 0.6f;
    float by = cy - eye_.h.x / 2 - 26 - browRaise_.x;
    float half = eye_.w.x * 0.48f;
    float dx = half * cosf(ang), dy = half * sinf(ang);
    float ix = bx - side * dx, iy = by + dy;  // binnenkant (+hoek = binnenkant omlaag = boos)
    float ox = bx + side * dx, oy = by - dy;
    if (bs == BrowStyle::Pixel) {
      for (int i = 0; i < 4; i++) {
        float t = i / 3.0f;
        float px = ix + (ox - ix) * t, py = iy + (oy - iy) * t;
        fillRect(layer, px - 6, py - 6, px + 6, py + 6, 2, col);
      }
    } else {
      line(layer, ix, iy, ox, oy, width, col);
    }
  }
}

void Face::drawRobotGlasses(lv_layer_t* layer, float cy) {
  const GlassesStyle gs = (GlassesStyle)look_.glasses;
  if (gs == GlassesStyle::None) return;
  const lv_color_t fr = hex(look_.frame);
  float hw = eye_.w.x / 2 + 18, hh = eye_.h.x / 2 + 14;
  if (gs == GlassesStyle::Monocle) {
    float ex = C + kEyeSpacing + lookX_.x * 0.5f;
    float rr = LV_MAX(hw, hh);
    frame(layer, ex - rr, cy - rr, ex + rr, cy + rr, LV_RADIUS_CIRCLE, 6, fr, lv_color_black(), 0);
    line(layer, ex + rr * 0.7f, cy + rr * 0.7f, ex + rr * 0.95f, cy + rr * 1.9f, 3, fr, 200);  // kettinkje
    return;
  }
  float rad = gs == GlassesStyle::Round ? LV_RADIUS_CIRCLE : 14;
  for (int side = -1; side <= 1; side += 2) {
    float ex = C + side * kEyeSpacing + lookX_.x * 0.5f;
    if (gs == GlassesStyle::Shades) {
      // donkere glazen: de ogen schijnen er gedempt doorheen
      frame(layer, ex - hw, cy - hh, ex + hw, cy + hh * 0.9f, 22, 7, fr, lv_color_hex(0x0A0A0A), 200);
      line(layer, ex - hw * 0.6f, cy - hh * 0.55f, ex - hw * 0.2f, cy - hh * 0.75f, 5, lv_color_white(), 110);
    } else {
      frame(layer, ex - hw, cy - hh, ex + hw, cy + hh, rad, 6, fr, lv_color_black(), 0);
    }
  }
  float yb = cy - hh * 0.35f;
  line(layer, C - kEyeSpacing + hw + lookX_.x * 0.5f, yb, C + kEyeSpacing - hw + lookX_.x * 0.5f, yb, 6, fr);
}

void Face::drawMouth(lv_layer_t* layer, float cx, float cy) {
  const lv_color_t col = lv_color_make((uint8_t)colR_.x, (uint8_t)colG_.x, (uint8_t)colB_.x);
  const MouthStyle ms = (MouthStyle)look_.mouth;
  lv_opa_t opa = ms == MouthStyle::Auto ? (lv_opa_t)(LV_CLAMP(0.f, mouthAlpha_, 1.f) * 255) : LV_OPA_COVER;
  // vorm per emotie: glimlach / frons / o / plat / geen
  uint8_t shape = mouthShown_;

  if (ms == MouthStyle::Line) {
    float w = shape == MouthSmile ? 34 : (shape == MouthFrown ? 18 : (shape == MouthO ? 12 : 26));
    fillRect(layer, cx - w, cy + 108, cx + w, cy + 117, 5, col);
    return;
  }
  if (ms == MouthStyle::Zigzag) {
    float amp = shape == MouthSmile ? 9 : (shape == MouthO ? 12 : 6);
    float w = shape == MouthSmile ? 42 : 32, y = cy + 112 + (shape == MouthFrown ? 6 : 0);
    float px = cx - w, py = y;
    for (int i = 1; i <= 6; i++) {
      float x = cx - w + i * (2 * w / 6), yy = y + ((i & 1) ? -amp : amp);
      line(layer, px, py, x, yy, 6, col);
      px = x;
      py = yy;
    }
    return;
  }
  if (ms == MouthStyle::Equalizer) {
    // LED-balkjes; bewegen mee met het geluid, in rust een rustig golfje
    static const float bell[7] = {0.35f, 0.6f, 0.85f, 1.0f, 0.85f, 0.6f, 0.35f};
    float lvl = LV_CLAMP(0.f, soundLevel_, 1.f);
    for (int i = 0; i < 7; i++) {
      float wob = 0.5f + 0.5f * sinf(t_ * 9 + i * 1.3f);
      float hgt = 6 + bell[i] * (lvl * 46 * (0.6f + 0.4f * wob) + (shape == MouthSmile ? 10 : 4));
      float x = cx - 45 + i * 15;
      fillRect(layer, x - 5, cy + 114 - hgt / 2, x + 5, cy + 114 + hgt / 2, 2, col);
    }
    return;
  }
  if (ms == MouthStyle::Pixel) {
    auto px = [&](float x, float y) { fillRect(layer, x - 6, y - 6, x + 6, y + 6, 2, col); };
    switch (shape) {
      case MouthSmile:
        for (int i = -3; i <= 3; i++) px(cx + i * 14, cy + 112 - (abs(i) >= 2 ? (abs(i) - 1) * 12 : 0));
        break;
      case MouthFrown:
        for (int i = -2; i <= 2; i++) px(cx + i * 14, cy + 112 + (abs(i) >= 2 ? 12 : 0) - 8);
        break;
      case MouthO:
        for (int i = 0; i < 8; i++) px(cx + cosf(i * kPi / 4) * 16, cy + 112 + sinf(i * kPi / 4) * 16);
        break;
      default:
        for (int i = -2; i <= 2; i++) px(cx + i * 14, cy + 112);
        break;
    }
    return;
  }

  // Auto: de klassieke vormen
  lv_draw_arc_dsc_t a;
  lv_draw_arc_dsc_init(&a);
  a.color = col;
  a.opa = opa;
  a.width = 9;
  a.rounded = 1;
  switch (shape) {
    case MouthSmile:
      a.center = {ri(cx), ri(cy + 78)};
      a.radius = 34;
      a.start_angle = 35;
      a.end_angle = 145;
      lv_draw_arc(layer, &a);
      break;
    case MouthFrown:
      a.center = {ri(cx), ri(cy + 142)};
      a.radius = 30;
      a.start_angle = 220;
      a.end_angle = 320;
      lv_draw_arc(layer, &a);
      break;
    case MouthO: {
      float s = 1.0f + 0.08f * sinf(t_ * 5);
      a.center = {ri(cx), ri(cy + 112)};
      a.radius = (uint16_t)(15 * s);
      a.width = 8;
      a.start_angle = 0;
      a.end_angle = 360;
      lv_draw_arc(layer, &a);
      break;
    }
    case MouthFlat:
      fillRect(layer, cx - 24, cy + 108, cx + 24, cy + 117, 5, col, opa);
      break;
    default:
      break;
  }
}

}  // namespace buddy
