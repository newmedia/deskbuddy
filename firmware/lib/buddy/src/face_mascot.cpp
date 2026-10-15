// Mascotte-skin: cartoongezicht met stekelhaar, dikke wenkbrauwen, bril, witte ogen met iris en
// een grote open lach. Gebruikt dezelfde veren/emoties als de classic-skin (face.cpp).

#include <math.h>

#include "draw_util.h"
#include "face.h"

namespace buddy {

using namespace draw;

namespace {

constexpr float C = Face::kSize / 2.0f;
constexpr float kPi = 3.14159265f;
constexpr float kEyeDx = 82, kEyeY = 246;
constexpr float kHairUp = 44;  // haar hoger zodat de wenkbrauwen vrij blijven
constexpr float kLookX = 58, kLookY = 42;  // gelijk aan face.cpp

inline lv_color_t hex(uint32_t c) { return lv_color_hex(c); }
#define SKIN hex(0xF5C99B)
#define HAIR hex(0x0B2290)
#define HAIR_HI hex(0x3159E6)
#define OUTLINE hex(0x111522)
#define GLASS hex(0x15171C)
#define WHITE hex(0xFFFFFF)
#define IRIS hex(0x5A7CEB)
#define IRIS_DARK hex(0x22378F)
#define PUPIL hex(0x0A0F1F)
#define MOUTH hex(0x5A0F1C)
#define TONGUE hex(0xE0485E)
#define BLUSH hex(0xF08080)
#define HEART hex(0xFF4F8B)

struct MascotPose {
  float browAngle;  // graden, + = binnenkant omlaag (boos)
  float browRaise;  // px omhoog
  float mouthOpen;  // 0 = dicht, 1 = wijd open
  float mouthWidth;
};

MascotPose mascotPose(Emotion e) {
  switch (e) {
    case Emotion::Happy:     return {-6, 10, 1.00f, 180};
    case Emotion::Sad:       return {-20, 4, 0.00f, 90};
    case Emotion::Angry:     return {24, -8, 0.32f, 130};
    case Emotion::Surprised: return {0, 24, 1.00f, 70};
    case Emotion::Sleepy:    return {-4, -6, 0.00f, 80};
    case Emotion::Love:      return {-10, 12, 0.85f, 165};
    case Emotion::Dizzy:     return {6, 8, 1.00f, 60};
    case Emotion::Asleep:    return {0, -8, 0.00f, 70};
    case Emotion::Neutral:
    default:                 return {0, 0, 0.40f, 150};
  }
}

bool ovalMouth(Emotion e) { return e == Emotion::Surprised || e == Emotion::Dizzy; }

float ellipseHalfWidth(float halfW, float halfH, float dy) {
  float r = dy / halfH;
  return r >= 1 || r <= -1 ? 0 : halfW * sqrtf(1 - r * r);
}


}  // namespace

void Face::mascotTargets() {
  MascotPose p = mascotPose(emotion_);
  browAngle_.target = p.browAngle;
  browRaise_.target = p.browRaise;
  mouthOpen_.target = p.mouthOpen;
  mouthWidth_.target = p.mouthWidth;
}

void Face::drawMascot(lv_layer_t* layer) {
  // Geen ademhalings-deining: dan staan haar, achtergrond en bril stil en hoeven we per frame alleen
  // ogen, wenkbrauwen en mond te hertekenen (zie Face::invalidate). Scheelt veel CPU.
  const float bob = 0;
  float sq = LV_CLAMP(-0.6f, squash_.x, 0.6f);

  fillRect(layer, 0, 0, kSize, kSize, 0, SKIN);

  if (blush_.x > 0.02f) {
    lv_opa_t o = (lv_opa_t)LV_CLAMP(0.f, blush_.x * 150, 255.f);
    fillEllipse(layer, C - 128, 302 + bob, 66, 30, BLUSH, o);
    fillEllipse(layer, C + 128, 302 + bob, 66, 30, BLUSH, o);
  }

  float cy = kEyeY + bob + dy_.x * 0.3f;
  for (int side = -1; side <= 1; side += 2) drawMascotEye(layer, side, C + side * kEyeDx, cy);

  // Bril: twee glazen, brug en poten
  for (int side = -1; side <= 1; side += 2) {
    float ex = C + side * kEyeDx;
    frame(layer, ex - 66, cy - 50, ex + 66, cy + 48, 22, 9, GLASS, WHITE, 22);
    line(layer, ex - 44, cy - 30, ex - 28, cy - 40, 5, WHITE, 110);  // glinstering
    line(layer, ex + side * 62, cy - 30, C + side * 222, cy - 44, 9, GLASS);
  }
  line(layer, C - kEyeDx + 62, cy - 14, C + kEyeDx - 62, cy - 14, 9, GLASS);

  drawMascotMouth(layer, C + lookX_.x * 0.25f, 340 + bob + dy_.x * 0.4f + sq * 6);

  // Wenkbrauwen: dikke balken met outline, kantelen en bewegen mee met de emotie.
  float a = browAngle_.x * kPi / 180.0f;
  for (int side = -1; side <= 1; side += 2) {
    float bx = C + side * 86, by = cy - 78 - browRaise_.x;
    float dx = 46 * cosf(a), dy = 46 * sinf(a);
    float ix = bx - side * dx, iy = by + dy;  // binnenkant
    float ox = bx + side * dx, oy = by - dy;  // buitenkant
    line(layer, ix, iy, ox, oy, 28, OUTLINE);
    line(layer, ix, iy, ox, oy, 19, HAIR);
  }

  drawHair(layer, hairY_.x);
  if (emotion_ == Emotion::Sleepy || emotion_ == Emotion::Asleep) drawZs(layer, HAIR);
}

void Face::drawMascotEye(lv_layer_t* layer, int side, float cx, float cy) {
  float sw = LV_CLAMP(60.f, eye_.w.x * 0.92f, 112.f) * swell_;
  float sh = LV_CLAMP(44.f, eye_.h.x * 0.60f, 92.f) * swell_;
  float hw = sw / 2, hh = sh / 2;

  fillEllipse(layer, cx, cy, sw + 7, sh + 7, OUTLINE);
  fillEllipse(layer, cx, cy, sw, sh, WHITE);

  // Iris blijft binnen het oogwit
  float ir = LV_MIN(hh - 4, 25.0f);
  float ox = lookX_.x / kLookX * (hw - ir - 6);
  float oy = lookY_.x / kLookY * (hh - ir - 2);
  if (emotion_ == Emotion::Dizzy) {
    float w = t_ * 7.0f * side;
    ox = cosf(w) * (hw - ir - 6);
    oy = sinf(w) * (hh - ir - 2);
  }
  float maxX = ellipseHalfWidth(hw, hh, oy) - ir - 2;
  ox = LV_CLAMP(-maxX, ox, maxX);
  float ix = cx + ox, iy = cy + oy;
  if (heartShown_) {
    float pulse = 1.0f + 0.07f * sinf(t_ * 9.0f);
    heart(layer, ix, iy, ir * 2.3f * pulse, HEART);
  } else {
    fillEllipse(layer, ix, iy, 2 * ir, 2 * ir, IRIS_DARK);
    fillEllipse(layer, ix, iy, 2 * ir * 0.8f, 2 * ir * 0.8f, IRIS);
    fillEllipse(layer, ix, iy, ir * 0.95f, ir * 0.95f, PUPIL);
    fillEllipse(layer, ix - ir * 0.38f, iy - ir * 0.38f, ir * 0.42f, ir * 0.42f, WHITE);
  }

  // Bovenooglid in huidkleur (emotie-lid of knipper, wat het verst dicht is) met een wimperlijn.
  float c = eyeClosure(side);
  float lid = LV_MAX(LV_CLAMP(0.f, eye_.lidTop.x, 1.f), c);
  float tilt = eye_.tilt.x * 0.8f;
  if (lid > 0.01f || fabsf(tilt) > 0.5f) {
    float slope = tanf(tilt * kPi / 180.0f) * (hw + 6);
    float base = cy - hh + lid * sh;
    float yInner = base + slope, yOuter = base - slope;
    float yl = side < 0 ? yOuter : yInner, yr = side < 0 ? yInner : yOuter;
    float top = cy - hh - 8;
    yl = LV_MAX(yl, top);
    yr = LV_MAX(yr, top);
    float xl = cx - hw - 6, xr = cx + hw + 6;
    fillRect(layer, xl, top, xr, LV_MIN(yl, yr) + 2, 0, SKIN);
    if (yl < yr) fillTri(layer, xl, yl, xr, yl, xr, yr, SKIN);
    else if (yr < yl) fillTri(layer, xl, yr, xr, yr, xl, yl, SKIN);
    // wimperlijn alleen binnen de ooromtrek
    float wl = ellipseHalfWidth(hw + 3, hh + 3, yl - cy), wr = ellipseHalfWidth(hw + 3, hh + 3, yr - cy);
    if (lid > 0.97f) wl = wr = hw;  // helemaal dicht: volle lijn
    if (wl > 2 || wr > 2) {
      float k = (yr - yl) / (xr - xl);
      float x1 = cx - wl, x2 = cx + wr;
      line(layer, x1, yl + k * (x1 - xl), x2, yl + k * (x2 - xl), 6, OUTLINE);
    }
  }

  // Onderooglid (blije "^"-ogen)
  float lidB = LV_CLAMP(0.f, eye_.lidBottom.x, 1.f);
  if (lidB > 0.01f) {
    float ew = sw * 1.7f, eh = sh * 1.3f;
    float topY = cy + hh - lidB * sh;
    fillEllipse(layer, cx, topY + eh / 2, ew, eh, SKIN);
  }
}

void Face::drawMascotMouth(lv_layer_t* layer, float cx, float my) {
  if (pucker_) {
    drawPucker(layer, cx, my + 12, OUTLINE, true);
    return;
  }
  float o = LV_CLAMP(0.f, mouthOpen_.x, 1.2f);
  float mw = mouthWidth_.x;

  if (ovalMouth(emotion_)) {
    float s = 1.0f + 0.06f * sinf(t_ * 5);
    float ow = mw * 0.95f * s, oh = mw * 1.1f * s;
    fillEllipse(layer, cx, my + 20, ow + 9, oh + 9, OUTLINE);
    fillEllipse(layer, cx, my + 20, ow, oh, MOUTH);
    fillEllipse(layer, cx, my + 20 + oh * 0.28f, ow * 0.6f, oh * 0.32f, TONGUE);
    return;
  }

  if (o < 0.08f) {
    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = OUTLINE;
    a.width = 8;
    a.rounded = 1;
    if (emotion_ == Emotion::Sad) {
      a.center = {ri(cx), ri(my + mw * 0.75f)};
      a.radius = (uint16_t)(mw * 0.6f);
      a.start_angle = 228;
      a.end_angle = 312;
    } else {
      a.center = {ri(cx), ri(my - mw * 0.55f)};
      a.radius = (uint16_t)(mw * 0.95f);
      a.start_angle = 58;
      a.end_angle = 122;
    }
    lv_draw_arc(layer, &a);
    return;
  }

  // Open "D"-lach: alleen de onderste helft van ellipsen tekenen (clip-gebied vanaf de bovenlip).
  float h = 16 + 70 * o;
  lv_area_t saved = layer->_clip_area;
  lv_area_t clip = saved;
  clip.y1 = LV_MAX(clip.y1, ri(my));
  if (clip.y1 <= clip.y2) {
    layer->_clip_area = clip;
    fillEllipse(layer, cx, my, mw + 10, 2 * h + 10, OUTLINE);
    fillEllipse(layer, cx, my, mw, 2 * h, MOUTH);
    bool gritted = emotion_ == Emotion::Angry;
    float th = gritted ? 2 * h * 0.92f : 2 * LV_MIN(h * 0.42f, 30.0f);
    fillEllipse(layer, cx, my + 2, mw * (gritted ? 0.92f : 0.84f), th, WHITE);
    if (!gritted) fillEllipse(layer, cx, my + h * 0.68f, mw * 0.5f, h * 0.6f, TONGUE);
    layer->_clip_area = saved;
  }
  line(layer, cx - mw / 2 - 4, my, cx + mw / 2 + 4, my, 7, OUTLINE);
  // mondhoekjes
  line(layer, cx - mw / 2 - 4, my, cx - mw / 2 - 14, my - 10, 6, OUTLINE);
  line(layer, cx + mw / 2 + 4, my, cx + mw / 2 + 14, my - 10, 6, OUTLINE);
}

void Face::drawHair(lv_layer_t* layer, float dy) {
  dy -= kHairUp;
  // Pony-pieken (hangen naar beneden en zwiepen naar links) + bakkebaarden.
  static const float spikes[][6] = {
      {110, 92, 185, 118, 92, 182},  {170, 116, 245, 128, 150, 198}, {232, 124, 300, 122, 214, 190},
      {288, 120, 352, 104, 280, 176}, {340, 100, 400, 78, 362, 150},  {30, 118, 96, 104, 56, 220},
      {376, 84, 438, 120, 418, 208},
  };
  const float offs[][2] = {{0, 5}};

  fillEllipse(layer, C, -14 + dy, 632, 312, OUTLINE);
  for (auto& o : offs)
    for (auto& s : spikes)
      fillTri(layer, s[0] + o[0], s[1] + dy + o[1], s[2] + o[0], s[3] + dy + o[1], s[4] + o[0], s[5] + dy + o[1],
              OUTLINE);
  fillEllipse(layer, C, -14 + dy, 620, 300, HAIR);
  for (auto& s : spikes) fillTri(layer, s[0], s[1] + dy, s[2], s[3] + dy, s[4], s[5] + dy, HAIR);

  // Glanslijnen
  line(layer, 140, 40 + dy, 178, 112 + dy, 10, HAIR_HI, 200);
  line(layer, 212, 26 + dy, 236, 104 + dy, 9, HAIR_HI, 200);
  line(layer, 290, 36 + dy, 300, 98 + dy, 8, HAIR_HI, 200);
  line(layer, 345, 60 + dy, 352, 92 + dy, 6, HAIR_HI, 170);
}

}  // namespace buddy
