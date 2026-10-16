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
// Kleuren komen uit de look (huid, haar, iris, montuur); de rest is vast.
inline lv_color_t shade(uint32_t c, int toWhite) {  // >0 lichter, <0 donkerder (0..255)
  return toWhite >= 0 ? lv_color_mix(lv_color_white(), lv_color_hex(c), (uint8_t)toWhite)
                      : lv_color_mix(lv_color_black(), lv_color_hex(c), (uint8_t)(-toWhite));
}
#define SKIN hex(look_.face)
#define HAIR hex(look_.accent)
#define HAIR_HI shade(look_.accent, 70)
#define OUTLINE hex(0x111522)
#define GLASS hex(look_.frame)
#define WHITE hex(0xFFFFFF)
#define IRIS hex(look_.primary)
#define IRIS_DARK shade(look_.primary, -110)
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
    case Emotion::Cry:       return {-24, 6, 0.00f, 100};
    case Emotion::Hyper:     return {-8, 18, 1.00f, 195};
    case Emotion::Pain:      return {-26, -2, 0.34f, 140};
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

  {
    NoShift ns;  // de huid vult altijd het hele scherm
    fillRect(layer, 0, 0, kSize, kSize, 0, SKIN);
  }

  const CheekStyle cs = (CheekStyle)look_.cheeks;
  if (cs == CheekStyle::Blush && blush_.x > 0.02f) {
    lv_opa_t o = (lv_opa_t)LV_CLAMP(0.f, blush_.x * 150, 255.f);
    fillEllipse(layer, C - 128, 302 + bob, 66, 30, BLUSH, o);
    fillEllipse(layer, C + 128, 302 + bob, 66, 30, BLUSH, o);
  } else if (cs == CheekStyle::Freckles) {
    static const float fr[][2] = {{-18, -6}, {0, 4}, {16, -8}, {6, -16}, {-8, 12}};
    for (int side = -1; side <= 1; side += 2)
      for (auto& f : fr) fillEllipse(layer, C + side * 126 + f[0] * side, 300 + f[1], 7, 7, shade(look_.face, -90), 220);
  }

  float cy = kEyeY + bob + dy_.x * 0.3f;
  for (int side = -1; side <= 1; side += 2) drawMascotEye(layer, side, C + side * kEyeDx, cy);

  // Bril (stijl uit de look): glazen, brug en poten
  const GlassesStyle gs = (GlassesStyle)look_.glasses;
  if (gs == GlassesStyle::Monocle) {
    float ex = C + kEyeDx;
    frame(layer, ex - 62, cy - 58, ex + 62, cy + 58, LV_RADIUS_CIRCLE, 8, GLASS, WHITE, 22);
    line(layer, ex - 30, cy - 34, ex - 14, cy - 44, 5, WHITE, 110);
    line(layer, ex + 44, cy + 44, ex + 70, cy + 150, 4, GLASS);  // kettinkje
  } else if (gs != GlassesStyle::None) {
    bool round = gs == GlassesStyle::Round, shades = gs == GlassesStyle::Shades;
    for (int side = -1; side <= 1; side += 2) {
      float ex = C + side * kEyeDx;
      if (round) frame(layer, ex - 60, cy - 56, ex + 60, cy + 56, LV_RADIUS_CIRCLE, 8, GLASS, WHITE, 22);
      else frame(layer, ex - 66, cy - 50, ex + 66, cy + 48, 22, 9, GLASS, shades ? hex(0x101010) : WHITE, shades ? 215 : 22);
      line(layer, ex - 44, cy - 30, ex - 28, cy - 40, 5, WHITE, shades ? 140 : 110);  // glinstering
      line(layer, ex + side * (round ? 58 : 62), cy - 30, C + side * 222, cy - 44, 9, GLASS);
    }
    line(layer, C - kEyeDx + (round ? 56 : 62), cy - 14, C + kEyeDx - (round ? 56 : 62), cy - 14, 9, GLASS);
  }

  drawMascotMouth(layer, C + lookX_.x * 0.25f, 340 + bob + dy_.x * 0.4f + sq * 6);

  // Wenkbrauwen: dikke balken met outline, kantelen en bewegen mee met de emotie.
  float a = browAngle_.x * kPi / 180.0f;
  // Wenkbrauwen in de haarkleur; met een petje (haarkleur = petkleur) een donkere tint van de huid.
  const lv_color_t browCol = (HairStyle)look_.hair == HairStyle::Cap ? shade(look_.face, -150) : HAIR;
  for (int side = -1; side <= 1; side += 2) {
    float bx = C + side * 86, by = cy - 78 - browRaise_.x;
    float dx = 46 * cosf(a), dy = 46 * sinf(a);
    float ix = bx - side * dx, iy = by + dy;  // binnenkant
    float ox = bx + side * dx, oy = by - dy;  // buitenkant
    const BrowStyle bs = (BrowStyle)look_.brows;
    if (bs == BrowStyle::None) continue;
    if (bs == BrowStyle::Thin) {
      line(layer, ix, iy, ox, oy, 12, OUTLINE);
      line(layer, ix, iy, ox, oy, 6, browCol);
    } else {
      float w = bs == BrowStyle::Bar ? 22 : 28;
      line(layer, ix, iy, ox, oy, w, OUTLINE);
      line(layer, ix, iy, ox, oy, w - 9, browCol);
    }
  }

  drawHair(layer, hairY_.x);

  if (earPop_ > 0.01f) drawEar(layer, true);  // luisteren: oor steekt uit (oorbel hangt eraan)

  // Oorbellen (onder het haar zichtbaar bij de oren)
  const EarringStyle er = (EarringStyle)look_.earrings;
  if (er != EarringStyle::None) {
    for (int side = -1; side <= 1; side += 2) {
      float x = C + side * 192, y = 318;
      if (er == EarringStyle::Studs) {
        fillEllipse(layer, x, y, 18, 18, OUTLINE);
        fillEllipse(layer, x, y, 13, 13, hex(look_.extra));
        fillEllipse(layer, x - 2, y - 2, 4, 4, WHITE, 200);
      } else {
        frame(layer, x - 17, y - 4, x + 17, y + 32, LV_RADIUS_CIRCLE, 5, hex(look_.extra), WHITE, 0);
      }
    }
  }
  // Haaraccessoire
  const lv_color_t ac = hex(look_.extra);
  float hy = hairY_.x - kHairUp;
  switch ((AccessoryStyle)look_.accessory) {
    case AccessoryStyle::Bow: {
      float x = C + 118, y = 104 + hy;
      fillTri(layer, x, y, x - 46, y - 28, x - 46, y + 28, OUTLINE);
      fillTri(layer, x, y, x + 46, y - 28, x + 46, y + 28, OUTLINE);
      fillTri(layer, x, y, x - 40, y - 22, x - 40, y + 22, ac);
      fillTri(layer, x, y, x + 40, y - 22, x + 40, y + 22, ac);
      fillEllipse(layer, x, y, 22, 22, OUTLINE);
      fillEllipse(layer, x, y, 16, 16, shade(look_.extra, 60));
      break;
    }
    case AccessoryStyle::Headband: {
      lv_draw_arc_dsc_t a;
      lv_draw_arc_dsc_init(&a);
      a.center = {ri(C + ox), ri(250 + hy + oy)};
      a.radius = 214;
      a.start_angle = 205;
      a.end_angle = 335;
      a.width = 20;
      a.color = OUTLINE;
      lv_draw_arc(layer, &a);
      a.width = 14;
      a.radius = 211;
      a.color = ac;
      lv_draw_arc(layer, &a);
      break;
    }
    case AccessoryStyle::Flower: {
      float x = C - 132, y = 104 + hy;
      for (int i = 0; i < 5; i++) {
        float an = i * 2 * kPi / 5;
        fillEllipse(layer, x + cosf(an) * 18, y + sinf(an) * 18, 30, 30, OUTLINE);
      }
      for (int i = 0; i < 5; i++) {
        float an = i * 2 * kPi / 5;
        fillEllipse(layer, x + cosf(an) * 18, y + sinf(an) * 18, 24, 24, ac);
      }
      fillEllipse(layer, x, y, 18, 18, hex(0xFFD54A));
      break;
    }
    default:
      break;
  }
  // Tranen als laatste: lang haar valt anders over de wangen heen
  if (emotion_ == Emotion::Cry) {
    float sh = LV_CLAMP(44.f, eye_.h.x * 0.60f, 92.f) * swell_;
    for (int side = -1; side <= 1; side += 2) drawTears(layer, C + side * kEyeDx, cy + sh / 2 - 6, side);
  }
  if (emotion_ == Emotion::Pain) drawSweat(layer, C + 172, 196);
  // Z'jes in een vaste lichte kleur: in de haarkleur verdwenen ze tegen het haar
  if (emotion_ == Emotion::Sleepy || emotion_ == Emotion::Asleep) drawZs(layer, hex(0xB8A6FF));
}

void Face::drawMascotEye(lv_layer_t* layer, int side, float cx, float cy) {
  float sw = LV_CLAMP(60.f, eye_.w.x * 0.92f, 112.f) * swell_;
  float sh = LV_CLAMP(44.f, eye_.h.x * 0.60f, 92.f) * swell_;
  float hw = sw / 2, hh = sh / 2;

  const bool dots = (EyeStyle)look_.eyes == EyeStyle::Round;  // stipjes-ogen: geen oogwit
  if (!dots) {
    fillEllipse(layer, cx, cy, sw + 7, sh + 7, OUTLINE);
    fillEllipse(layer, cx, cy, sw, sh, WHITE);
  }

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
  } else if (dots) {
    fillEllipse(layer, ix, iy, ir * 1.5f, ir * 1.9f, IRIS_DARK);
    fillEllipse(layer, ix - ir * 0.25f, iy - ir * 0.45f, ir * 0.5f, ir * 0.5f, WHITE);
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
    if (lid > 0.97f) {
      // Helemaal dicht: een gesloten-oog-boogje in het midden (onderaan viel de lijn weg tegen de bril)
      lv_draw_arc_dsc_t d;
      lv_draw_arc_dsc_init(&d);
      d.center = {ri(cx + ox), ri(cy - hw * 0.55f + oy)};
      d.radius = (uint16_t)(hw * 0.95f);
      d.start_angle = 55;
      d.end_angle = 125;
      d.width = 6;
      d.rounded = 1;
      d.color = OUTLINE;
      lv_draw_arc(layer, &d);
    } else if (wl > 2 || wr > 2) {
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
  // Wimpers of eyeliner aan de buitenkant (alleen als het oog open genoeg is)
  const LashStyle ls = (LashStyle)look_.lashes;
  if (ls != LashStyle::None && LV_MAX(c, LV_CLAMP(0.f, eye_.lidTop.x, 1.f)) < 0.55f) {
    if (ls == LashStyle::Liner) {
      line(layer, cx + side * hw * 0.35f, cy - hh * 0.95f, cx + side * (hw + 4), cy - hh * 0.55f, 7, OUTLINE);
      line(layer, cx + side * (hw + 2), cy - hh * 0.55f, cx + side * (hw + 20), cy - hh * 0.95f, 7, OUTLINE);
    } else {
      static const float ang[] = {0.45f, 0.8f, 1.15f};  // radialen vanaf horizontaal, aan de buitenkant
      for (float a : ang) {
        float px = cx + side * (hw + 2) * cosf(a), py = cy - (hh + 2) * sinf(a);
        line(layer, px, py, px + side * 13 * cosf(a), py - 13 * sinf(a), 5, OUTLINE);
      }
    }
  }
}

void Face::drawMascotMouth(lv_layer_t* layer, float cx, float my) {
  const bool lipstick = (LipStyle)look_.lips == LipStyle::Lipstick;
  const lv_color_t lipCol = lipstick ? hex(look_.lip) : OUTLINE;
  if (pucker_) {
    drawPucker(layer, cx, my + 12, OUTLINE, true);
    return;
  }
  float o = LV_CLAMP(0.f, mouthOpen_.x, 1.2f);
  if ((MouthStyle)look_.mouth == MouthStyle::Line) o = 0;  // altijd een gesloten glimlach
  float mw = mouthWidth_.x;

  if (emotion_ == Emotion::Pain) {
    // Grimas: tanden op elkaar in een smalle balk, mondhoeken omlaag
    float w = mw * 0.78f, h = 30, y = my + 6;
    fillRect(layer, cx - w / 2 - 5, y - h / 2 - 5, cx + w / 2 + 5, y + h / 2 + 5, 14, lipCol);
    fillRect(layer, cx - w / 2, y - h / 2, cx + w / 2, y + h / 2, 10, WHITE);
    line(layer, cx - w / 2 + 2, y, cx + w / 2 - 2, y, 3, OUTLINE);
    for (int i = 1; i < 5; i++) {
      float x = cx - w / 2 + w * i / 5;
      line(layer, x, y - h / 2 + 2, x, y + h / 2 - 2, 3, OUTLINE);
    }
    line(layer, cx - w / 2 - 4, y + 2, cx - w / 2 - 16, y + 14, 6, lipCol);
    line(layer, cx + w / 2 + 4, y + 2, cx + w / 2 + 16, y + 14, 6, lipCol);
    return;
  }
  if (ovalMouth(emotion_)) {
    float s = 1.0f + 0.06f * sinf(t_ * 5);
    float ow = mw * 0.95f * s, oh = mw * 1.1f * s;
    fillEllipse(layer, cx, my + 20, ow + 9 + (lipstick ? 6 : 0), oh + 9 + (lipstick ? 6 : 0), lipCol);
    fillEllipse(layer, cx, my + 20, ow, oh, MOUTH);
    fillEllipse(layer, cx, my + 20 + oh * 0.28f, ow * 0.6f, oh * 0.32f, TONGUE);
    return;
  }

  if (o < 0.08f) {
    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = lipCol;
    a.width = lipstick ? 11 : 8;
    a.rounded = 1;
    if (emotion_ == Emotion::Sad || emotion_ == Emotion::Cry) {
      a.center = {ri(cx + ox), ri(my + mw * 0.75f + oy)};
      a.radius = (uint16_t)(mw * 0.6f);
      a.start_angle = 228;
      a.end_angle = 312;
    } else {
      a.center = {ri(cx + ox), ri(my - mw * 0.55f + oy)};
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
  clip.y1 = LV_MAX(clip.y1, ri(my + oy));
  if (clip.y1 <= clip.y2) {
    layer->_clip_area = clip;
    fillEllipse(layer, cx, my, mw + 10 + (lipstick ? 6 : 0), 2 * h + 10 + (lipstick ? 6 : 0), lipCol);
    fillEllipse(layer, cx, my, mw, 2 * h, MOUTH);
    bool gritted = emotion_ == Emotion::Angry || emotion_ == Emotion::Pain;
    float th = gritted ? 2 * h * 0.92f : 2 * LV_MIN(h * 0.42f, 30.0f);
    fillEllipse(layer, cx, my + 2, mw * (gritted ? 0.92f : 0.84f), th, WHITE);
    if (!gritted) fillEllipse(layer, cx, my + h * 0.68f, mw * 0.5f, h * 0.6f, TONGUE);
    layer->_clip_area = saved;
  }
  line(layer, cx - mw / 2 - 4, my, cx + mw / 2 + 4, my, lipstick ? 11 : 7, lipCol);
  // mondhoekjes
  line(layer, cx - mw / 2 - 4, my, cx - mw / 2 - 14, my - 10, 6, lipCol);
  line(layer, cx + mw / 2 + 4, my, cx + mw / 2 + 14, my - 10, 6, lipCol);
}

void Face::drawHair(lv_layer_t* layer, float dy) {
  dy -= kHairUp;
  const HairStyle hs = (HairStyle)look_.hair;
  switch (hs) {
    case HairStyle::Neat: {
      // Glad, met een zijscheiding en een lok die over het voorhoofd zwiept
      fillEllipse(layer, C, -24 + dy, 640, 316, OUTLINE);
      fillEllipse(layer, C, -24 + dy, 628, 304, HAIR);
      fillTri(layer, 90, 96 + dy, 380, 100 + dy, 130, 176 + dy, OUTLINE);
      fillTri(layer, 96, 92 + dy, 374, 96 + dy, 134, 168 + dy, HAIR);
      fillTri(layer, 30, 110 + dy, 92, 100 + dy, 52, 210 + dy, HAIR);    // bakkebaarden
      fillTri(layer, 374, 100 + dy, 436, 110 + dy, 414, 210 + dy, HAIR);
      line(layer, 300, 30 + dy, 272, 100 + dy, 6, shade(look_.accent, -60), 200);  // scheiding
      line(layer, 160, 48 + dy, 220, 98 + dy, 9, HAIR_HI, 190);
      break;
    }
    case HairStyle::Curly: {
      // Krullen: rijen rondjes langs de bovenkant
      static const float curls[][3] = {
          {70, 150, 40},  {110, 104, 42}, {165, 76, 44}, {225, 64, 46}, {285, 70, 44}, {340, 92, 42}, {390, 132, 40},
          {120, 150, 34}, {180, 128, 36}, {240, 120, 38}, {300, 126, 36}, {350, 150, 34}, {40, 200, 34}, {426, 200, 34},
          {233, 20, 70},  {140, 30, 60},  {330, 30, 60},
      };
      for (auto& c : curls) fillEllipse(layer, c[0], c[1] + dy, 2 * c[2] + 10, 2 * c[2] + 10, OUTLINE);
      for (auto& c : curls) fillEllipse(layer, c[0], c[1] + dy, 2 * c[2], 2 * c[2], HAIR);
      for (auto& c : curls)
        fillEllipse(layer, c[0] - c[2] * 0.3f, c[1] + dy - c[2] * 0.3f, c[2] * 0.6f, c[2] * 0.6f, HAIR_HI, 120);
      break;
    }
    case HairStyle::Mohawk: {
      // Kale zijkanten (iets donkerder: stoppels), een kam in het midden
      static const float sp[][6] = {{180, 120, 286, 120, 200, -10}, {178, 80, 288, 80, 250, -40},
                                    {182, 40, 284, 40, 300, -20}, {196, 130, 270, 130, 226, 172}};
      fillRect(layer, 186, -20 + dy, 280, 132 + dy, 24, OUTLINE);
      for (auto& s : sp) fillTri(layer, s[0] - 5, s[1] + dy, s[2] + 5, s[3] + dy, s[4], s[5] + dy - 5, OUTLINE);
      fillRect(layer, 192, -20 + dy, 274, 126 + dy, 20, HAIR);
      for (auto& s : sp) fillTri(layer, s[0], s[1] + dy, s[2], s[3] + dy, s[4], s[5] + dy, HAIR);
      line(layer, 218, 0 + dy, 222, 110 + dy, 8, HAIR_HI, 190);
      break;
    }
    case HairStyle::Bald: {
      // Kaal met een glans, en plukjes boven de oren
      fillEllipse(layer, C - 70, 66 + dy + kHairUp, 120, 50, WHITE, 70);
      for (int side = -1; side <= 1; side += 2) {
        float x = C + side * 198;
        fillTri(layer, x - 22, 196, x + 22, 196, x + side * 8, 262, OUTLINE);
        fillTri(layer, x - 17, 199, x + 17, 199, x + side * 7, 254, HAIR);
        fillTri(layer, x - 24, 208, x + 20, 214, x + side * 18, 248, HAIR);
      }
      break;
    }
    case HairStyle::Cap: {
      // Petje: koepel in de haarkleur, klep naar voren-links, knoopje bovenop
      fillEllipse(layer, C, -10 + dy, 600, 300, OUTLINE);
      fillEllipse(layer, C, -10 + dy, 588, 288, HAIR);
      fillRect(layer, 30, 118 + dy, 330, 146 + dy, 14, OUTLINE);
      fillRect(layer, 34, 120 + dy, 326, 142 + dy, 12, shade(look_.accent, -70));
      fillEllipse(layer, C, -6 + dy, 30, 20, shade(look_.accent, -70));
      fillEllipse(layer, C + 60, 70 + dy, 56, 56, WHITE, 230);  // logo
      line(layer, 150, 30 + dy, 190, 100 + dy, 10, HAIR_HI, 180);
      break;
    }
    case HairStyle::Long:
    case HairStyle::LongCurly: {
      // Lang haar: kap + zachte pony + lange, ronde zijlokken langs de rand tot onderaan
      bool curly = hs == HairStyle::LongCurly;
      for (int pass = 0; pass < 2; pass++) {
        lv_color_t c = pass ? HAIR : OUTLINE;
        float g = pass ? 0 : 7;
        fillEllipse(layer, C, -20 + dy, 630 + g, 310 + g, c);
        fillEllipse(layer, 30, 280, 190 + g, 470 + g, c);    // zijlokken
        fillEllipse(layer, 436, 280, 190 + g, 470 + g, c);
        fillEllipse(layer, C - 40, 70 + dy, 300 + g, 150 + g, c);  // pony, iets naar links
        if (curly)
          for (int i = 0; i < 6; i++) {
            fillEllipse(layer, 74, 150 + i * 56, 66 + g, 66 + g, c);
            fillEllipse(layer, 392, 150 + i * 56, 66 + g, 66 + g, c);
          }
      }
      line(layer, 150, 44 + dy, 196, 96 + dy, 9, HAIR_HI, 180);
      line(layer, 34, 190, 40, 340, 8, HAIR_HI, 140);
      line(layer, 430, 190, 424, 340, 8, HAIR_HI, 140);
      break;
    }
    case HairStyle::Ponytail: {
      // Strak naar achteren met een paardenstaart rechts
      fillEllipse(layer, C, -20 + dy, 630, 310, OUTLINE);
      fillEllipse(layer, C, -20 + dy, 618, 298, HAIR);
      fillEllipse(layer, 420, 300, 86, 230, OUTLINE);
      fillEllipse(layer, 420, 300, 74, 218, HAIR);
      fillRect(layer, 396, 164, 444, 186, 8, hex(look_.extra));  // elastiekje
      fillTri(layer, 100, 96 + dy, 300, 96 + dy, 120, 132 + dy, HAIR);
      line(layer, 170, 40 + dy, 230, 90 + dy, 9, HAIR_HI, 180);
      line(layer, 412, 200, 418, 360, 7, HAIR_HI, 140);
      break;
    }
    case HairStyle::Bun: {
      // Knotje bovenop + gladde kap met kleine pony
      fillEllipse(layer, C, -20 + dy, 630, 310, OUTLINE);
      fillEllipse(layer, C, -20 + dy, 618, 298, HAIR);
      fillEllipse(layer, C, 52, 150, 120, OUTLINE);  // knotje: vast in beeld bovenop
      fillEllipse(layer, C, 52, 138, 108, HAIR);
      line(layer, C - 52, 54, C + 52, 54, 5, shade(look_.accent, -50), 200);
      fillTri(layer, 30, 110 + dy, 92, 100 + dy, 56, 200 + dy, HAIR);
      fillTri(layer, 374, 100 + dy, 436, 110 + dy, 410, 200 + dy, HAIR);
      line(layer, C - 34, 26, C - 4, 44, 7, HAIR_HI, 180);
      break;
    }
    case HairStyle::Bob: {
      // Bob tot de kaaklijn (ronde zijkanten) met een rechte pony
      for (int pass = 0; pass < 2; pass++) {
        lv_color_t c = pass ? HAIR : OUTLINE;
        float g = pass ? 0 : 7;
        fillEllipse(layer, C, -20 + dy, 630 + g, 310 + g, c);
        fillEllipse(layer, 46, 226, 190 + g, 300 + g, c);
        fillEllipse(layer, 420, 226, 190 + g, 300 + g, c);
        fillRect(layer, 96 - g, 70 + dy, 370 + g, 136 + dy + g, 24, c);  // pony
      }
      line(layer, 140, 40 + dy, 190, 100 + dy, 9, HAIR_HI, 180);
      break;
    }
    case HairStyle::Pigtails: {
      // Middenscheiding met twee staartjes opzij
      fillEllipse(layer, C, -20 + dy, 630, 310, OUTLINE);
      fillEllipse(layer, C, -20 + dy, 618, 298, HAIR);
      line(layer, C, 0 + dy, C, 96 + dy, 5, shade(look_.accent, -60), 200);
      for (int side = -1; side <= 1; side += 2) {
        float x = C + side * 206;
        fillEllipse(layer, x, 290, 98, 160, OUTLINE);
        fillEllipse(layer, x, 290, 86, 148, HAIR);
        fillEllipse(layer, x - side * 10, 196, 34, 24, hex(look_.extra));  // elastiekje
      }
      fillTri(layer, 96, 96 + dy, C - 6, 96 + dy, 116, 140 + dy, HAIR);
      fillTri(layer, C + 6, 96 + dy, 370, 96 + dy, 350, 140 + dy, HAIR);
      break;
    }
    case HairStyle::Spiky:
    default: {
      // Pony-pieken (hangen naar beneden en zwiepen naar links) + bakkebaarden.
      static const float spikes[][6] = {
          {110, 92, 185, 118, 92, 182},  {170, 116, 245, 128, 150, 198}, {232, 124, 300, 122, 214, 190},
          {288, 120, 352, 104, 280, 176}, {340, 100, 400, 78, 362, 150},  {30, 118, 96, 104, 56, 220},
          {376, 84, 438, 120, 418, 208},
      };
      fillEllipse(layer, C, -14 + dy, 632, 312, OUTLINE);
      for (auto& s : spikes) fillTri(layer, s[0], s[1] + dy + 5, s[2], s[3] + dy + 5, s[4], s[5] + dy + 5, OUTLINE);
      fillEllipse(layer, C, -14 + dy, 620, 300, HAIR);
      for (auto& s : spikes) fillTri(layer, s[0], s[1] + dy, s[2], s[3] + dy, s[4], s[5] + dy, HAIR);
      line(layer, 140, 40 + dy, 178, 112 + dy, 10, HAIR_HI, 200);
      line(layer, 212, 26 + dy, 236, 104 + dy, 9, HAIR_HI, 200);
      line(layer, 290, 36 + dy, 300, 98 + dy, 8, HAIR_HI, 200);
      line(layer, 345, 60 + dy, 352, 92 + dy, 6, HAIR_HI, 170);
      break;
    }
  }
}

}  // namespace buddy
