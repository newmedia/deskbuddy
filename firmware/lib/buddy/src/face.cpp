#include "face.h"

#include "draw_util.h"

#include <math.h>
#include <string.h>

#include <initializer_list>

namespace buddy {

using namespace draw;

namespace {

constexpr float kCenter = Face::kSize / 2.0f;
constexpr float kEyeSpacing = 94;   // afstand oogmidden tot schermmidden
constexpr float kEyeY = -18;        // ogen iets boven het midden
constexpr float kLookX = 58, kLookY = 42;
constexpr float kPi = 3.14159265f;
const lv_color_t kBlack = LV_COLOR_MAKE(0, 0, 0);

const char* const kNames[] = {"neutral", "happy", "sad",   "angry", "surprised",
                              "sleepy",  "love",  "dizzy", "asleep"};

void growBox(lv_area_t& box, float x1, float y1, float x2, float y2) {
  box.x1 = LV_MIN(box.x1, ri(x1));
  box.y1 = LV_MIN(box.y1, ri(y1));
  box.x2 = LV_MAX(box.x2, ri(x2));
  box.y2 = LV_MAX(box.y2, ri(y2));
}

}  // namespace

const char* skinName(Skin s) { return s == Skin::Mascot ? "mascot" : "classic"; }

bool skinFromName(const char* name, Skin& out) {
  if (!strcmp(name, "classic")) out = Skin::Classic;
  else if (!strcmp(name, "mascot")) out = Skin::Mascot;
  else return false;
  return true;
}

void Face::setSkin(Skin s) {
  if (s == skin_) return;
  skin_ = s;
  fullRedraw_ = true;
  if (obj_) lv_obj_invalidate(obj_);
}

const char* emotionName(Emotion e) {
  auto i = (unsigned)e;
  return i < (unsigned)Emotion::Count ? kNames[i] : "?";
}

bool emotionFromName(const char* name, Emotion& out) {
  for (unsigned i = 0; i < (unsigned)Emotion::Count; i++) {
    if (strcmp(name, kNames[i]) == 0) {
      out = (Emotion)i;
      return true;
    }
  }
  return false;
}

Face::Pose Face::poseFor(Emotion e) {
  //          w    h    r   lidT  tilt  lidB   dy  blush  color     mouth       heart
  switch (e) {
    case Emotion::Happy:     return {112, 124, 44, 0.00f,   0, 0.46f,  -6, 0.55f, 0x7CF5E8, MouthSmile, false};
    case Emotion::Sad:       return { 98, 112, 34, 0.30f, -16, 0.00f,  14, 0.00f, 0x6EA8FF, MouthFrown, false};
    case Emotion::Angry:     return {104, 108, 30, 0.30f,  24, 0.10f,   4, 0.00f, 0xFF5A4E, MouthFlat,  false};
    case Emotion::Surprised: return {118, 150, 52, 0.00f,   0, 0.00f, -10, 0.00f, 0x8AF3FF, MouthO,     false};
    case Emotion::Sleepy:    return {104, 124, 36, 0.52f,  -6, 0.08f,   8, 0.00f, 0x5FC9DA, MouthNone,  false};
    case Emotion::Love:      return {120, 120, 40, 0.00f,   0, 0.00f,  -4, 0.70f, 0xFF6FA8, MouthSmile, true};
    case Emotion::Dizzy:     return { 84,  84, 42, 0.00f,   0, 0.00f,   0, 0.00f, 0xB89CFF, MouthO,     false};
    case Emotion::Asleep:    return {104, 124, 36, 0.93f,   0, 0.04f,  14, 0.00f, 0x2E7F8C, MouthNone,  false};
    case Emotion::Neutral:
    default:                 return {100, 130, 34, 0.00f,   0, 0.00f,   0, 0.00f, 0x6EE7F7, MouthNone,  false};
  }
}

void Face::create(lv_obj_t* parent) {
  obj_ = lv_obj_create(parent);
  lv_obj_remove_style_all(obj_);
  lv_obj_set_size(obj_, kSize, kSize);
  lv_obj_align(obj_, LV_ALIGN_CENTER, 0, 0);
  lv_obj_remove_flag(obj_, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(obj_, drawCb, LV_EVENT_DRAW_MAIN, this);

  pose_ = poseFor(emotion_);
  eye_.w.snap(pose_.w);
  eye_.h.snap(pose_.h);
  eye_.r.snap(pose_.r);
  eye_.lidTop.snap(pose_.lidTop);
  eye_.tilt.snap(pose_.tilt);
  eye_.lidBottom.snap(pose_.lidBottom);
  dy_.snap(pose_.dy);
  blush_.snap(0);
  colR_.snap((pose_.color >> 16) & 0xff);
  colG_.snap((pose_.color >> 8) & 0xff);
  colB_.snap(pose_.color & 0xff);
  for (Spring* s : {&colR_, &colG_, &colB_, &blush_}) {
    s->k = 60;
    s->d = 16;
  }
  lookX_.k = lookY_.k = 260;
  lookX_.d = lookY_.d = 20;
  squash_.k = 320;
  squash_.d = 14;
  hairY_.k = 90;   // haar veert trager na dan het gezicht
  hairY_.d = 9;
  mascotTargets();
  browAngle_.snap(browAngle_.target);
  browRaise_.snap(browRaise_.target);
  mouthOpen_.snap(mouthOpen_.target);
  mouthWidth_.snap(mouthWidth_.target);
}

void Face::setEmotion(Emotion e) {
  if (e == emotion_) return;
  emotion_ = e;
  applyPose(poseFor(e));
  mascotTargets();
  if (e == Emotion::Surprised || e == Emotion::Happy || e == Emotion::Love) bounce(0.8f);
  if (e == Emotion::Asleep) lookAt(0, 0.1f, true);
  else if (lookHeld_ && emotion_ != Emotion::Asleep) releaseLook();
}

void Face::applyPose(const Pose& p) {
  pose_ = p;
  eye_.w.target = p.w;
  eye_.h.target = p.h;
  eye_.r.target = p.r;
  eye_.lidTop.target = p.lidTop;
  eye_.tilt.target = p.tilt;
  eye_.lidBottom.target = p.lidBottom;
  dy_.target = p.dy;
  blush_.target = p.blush;
  colR_.target = (p.color >> 16) & 0xff;
  colG_.target = (p.color >> 8) & 0xff;
  colB_.target = p.color & 0xff;
  // Vorm wisselen (hart <-> rond) doen we achter een knipper, dan zie je de overgang niet.
  if (p.heart != heartShown_) {
    pendingHeart_ = true;
    blink();
  }
}

void Face::lookAt(float x, float y, bool hold) {
  lookX_.target = LV_CLAMP(-1.0f, x, 1.0f) * kLookX;
  lookY_.target = LV_CLAMP(-1.0f, y, 1.0f) * kLookY;
  lookHeld_ = hold;
  if (!hold) scheduleNextSaccade();
}

void Face::releaseLook() {
  lookHeld_ = false;
  nextSaccade_ = t_ + 0.6f;
}

void Face::blink() {
  if (blinkT_ < 0) blinkT_ = 0;
}

void Face::bounce(float amount) { squash_.v += 9.0f * amount; }

float Face::rnd() {
  seed_ = seed_ * 1664525u + 1013904223u;
  return (seed_ >> 8) / 16777216.0f;
}

void Face::scheduleNextBlink() {
  float base = (emotion_ == Emotion::Sleepy) ? 1.6f : 2.2f;
  nextBlink_ = t_ + base + rnd() * 3.8f;
}

void Face::scheduleNextSaccade() { nextSaccade_ = t_ + 0.9f + rnd() * 2.8f; }

void Face::update(uint32_t dtMs) {
  float dt = LV_MIN(dtMs, 50u) / 1000.0f;
  t_ += dt;

  // Knipperen
  if (autoBlink_ && blinkT_ < 0 && t_ >= nextBlink_ && emotion_ != Emotion::Asleep) {
    blinkT_ = 0;
    if (rnd() < 0.15f) nextBlink_ = t_ + 0.25f;  // af en toe dubbel knipperen
    else scheduleNextBlink();
  }
  if (blinkT_ >= 0) {
    float dur = (emotion_ == Emotion::Sleepy) ? 0.32f : 0.16f;
    blinkT_ += dt / dur;
    if (pendingHeart_ && blinkT_ >= 0.45f) {
      heartShown_ = pose_.heart;
      pendingHeart_ = false;
    }
    if (blinkT_ >= 1) {
      blinkT_ = -1;
      if (nextBlink_ <= t_) scheduleNextBlink();
    }
  }

  // Rondkijken (saccades)
  if (!lookHeld_ && t_ >= nextSaccade_) {
    if (rnd() < 0.4f) {
      lookX_.target = lookY_.target = 0;
    } else {
      lookX_.target = (rnd() * 2 - 1) * 0.75f * kLookX;
      lookY_.target = (rnd() * 2 - 1) * 0.55f * kLookY;
    }
    scheduleNextSaccade();
  }

  // Mond in- en uitfaden
  if (pose_.mouth != mouthShown_) {
    mouthAlpha_ -= dt * 7;
    if (mouthAlpha_ <= 0) {
      mouthAlpha_ = 0;
      mouthShown_ = pose_.mouth;
    }
  } else if (mouthShown_ != MouthNone) {
    mouthAlpha_ = LV_MIN(1.0f, mouthAlpha_ + dt * 4);
  }

  for (Spring* s : {&eye_.w, &eye_.h, &eye_.r, &eye_.lidTop, &eye_.tilt, &eye_.lidBottom, &lookX_,
                    &lookY_, &dy_, &blush_, &squash_, &colR_, &colG_, &colB_, &browAngle_, &browRaise_,
                    &mouthOpen_, &mouthWidth_, &hairY_}) {
    s->step(dt);
  }
  updateAction(dt);
  updateParticles(dt);
  // Trillen: het hele gezichtsobject heen en weer zetten (goedkoop, geen hertekenen per pixel nodig)
  if (shake_ > 0.2f) lv_obj_set_pos(obj_, (int32_t)((rnd() * 2 - 1) * shake_), (int32_t)((rnd() * 2 - 1) * shake_));
  else if (lv_obj_get_x(obj_) != 0 || lv_obj_get_y(obj_) != 0) lv_obj_set_pos(obj_, 0, 0);

  squash_.target = 0;
  hairY_.target = sinf(t_ * 2 * kPi / 4.2f) * 4.5f - squash_.x * 8;

  invalidate();
}

static float closure(float blinkT) { return blinkT < 0 ? 0 : sinf(blinkT * kPi); }

void Face::invalidate() {
  if (!obj_) return;
  if (skin_ == Skin::Mascot || fullRedraw_ || effectsActive()) {
    // Mascotte beweegt over bijna het hele scherm (haar, wenkbrauwen, mond): alles opnieuw.
    lv_obj_invalidate(obj_);
    fullRedraw_ = false;
    prevBox_ = {0, 0, kSize - 1, kSize - 1};
    return;
  }
  lv_area_t box = {kSize, kSize, 0, 0};
  float lx = lookX_.x, ly = lookY_.x;
  float maxW = eye_.w.x * 1.35f, maxH = eye_.h.x * 1.25f;
  for (int side = -1; side <= 1; side += 2) {
    float cx = kCenter + side * kEyeSpacing + lx;
    float cy = kCenter + kEyeY + dy_.x + ly;
    growBox(box, cx - maxW / 2 - 30, cy - maxH / 2 - 34, cx + maxW / 2 + 30, cy + maxH / 2 + 60);
  }
  if (mouthShown_ != MouthNone || mouthAlpha_ > 0)
    growBox(box, kCenter - 60, kCenter + 60, kCenter + 60, kCenter + 160);
  if (emotion_ == Emotion::Sleepy || emotion_ == Emotion::Asleep)
    growBox(box, kCenter + 90, kCenter - 220, kCenter + 200, kCenter - 40);

  lv_area_t inv = box;
  growBox(inv, prevBox_.x1, prevBox_.y1, prevBox_.x2, prevBox_.y2);
  inv.x1 = LV_MAX(inv.x1, 0);
  inv.y1 = LV_MAX(inv.y1, 0);
  inv.x2 = LV_MIN(inv.x2, kSize - 1);
  inv.y2 = LV_MIN(inv.y2, kSize - 1);
  if (inv.x2 > inv.x1 && inv.y2 > inv.y1) lv_obj_invalidate_area(obj_, &inv);
  prevBox_ = box;
}

void Face::drawCb(lv_event_t* e) {
  auto* self = static_cast<Face*>(lv_event_get_user_data(e));
  self->draw(lv_event_get_layer(e));
}

void Face::draw(lv_layer_t* layer) {
  if (skin_ == Skin::Mascot) {
    drawMascot(layer);
    drawEffects(layer);
    return;
  }
  float bob = sinf(t_ * 2 * kPi / 4.2f) * 3.0f;
  float baseY = kCenter + kEyeY + dy_.x + lookY_.x + bob;
  for (int side = -1; side <= 1; side += 2) {
    // Perspectief: het oog aan de kant waar hij naartoe kijkt wordt iets groter.
    float persp = 1.0f + side * (lookX_.x / kLookX) * 0.07f;
    float cx = kCenter + side * kEyeSpacing + lookX_.x;
    float cy = baseY;
    if (emotion_ == Emotion::Dizzy) {
      float a = t_ * 7.0f * side;
      cx += cosf(a) * 16;
      cy += sinf(a) * 16;
    }
    drawEye(layer, side, cx, cy, persp);
  }
  if (pucker_)
    drawPucker(layer, kCenter + lookX_.x * 0.4f, kCenter + bob + 112,
               lv_color_make((uint8_t)colR_.x, (uint8_t)colG_.x, (uint8_t)colB_.x), false);
  else if (mouthShown_ != MouthNone && mouthAlpha_ > 0.01f)
    drawMouth(layer, kCenter + lookX_.x * 0.4f, kCenter + bob);
  if (emotion_ == Emotion::Sleepy || emotion_ == Emotion::Asleep)
    drawZs(layer, lv_color_make((uint8_t)colR_.x, (uint8_t)colG_.x, (uint8_t)colB_.x));
  drawEffects(layer);
}

void Face::drawEye(lv_layer_t* layer, int side, float cx, float cy, float scale) {
  lv_color_t col = lv_color_make((uint8_t)LV_CLAMP(0.f, colR_.x, 255.f),
                                 (uint8_t)LV_CLAMP(0.f, colG_.x, 255.f),
                                 (uint8_t)LV_CLAMP(0.f, colB_.x, 255.f));
  float sq = LV_CLAMP(-0.6f, squash_.x, 0.6f) * 0.25f;
  float pulse = (emotion_ == Emotion::Love) ? 1.0f + 0.06f * sinf(t_ * 9.0f) : 1.0f;
  float w = eye_.w.x * scale * (1 + sq) * pulse * swell_;
  float h = eye_.h.x * scale * (1 - sq) * pulse * swell_;
  float c = eyeClosure(side);
  float hv = LV_MAX(h * (1 - c * 0.94f), 8.0f);
  float vs = hv / h;  // verticale schaal door knipperen

  float x1 = cx - w / 2, x2 = cx + w / 2;
  float y1 = cy - hv / 2, y2 = cy + hv / 2;

  // Blosjes onder de buitenkant van het oog
  if (blush_.x > 0.02f) {
    float bx = cx + side * 18;
    fillEllipse(layer, bx, cy + eye_.h.x / 2 + 26, 74, 34, lv_color_hex(0xFF6FA8),
                (lv_opa_t)LV_CLAMP(0.f, blush_.x * 190, 255.f));
  }

  if (heartShown_) {
    float s = w * 1.08f;
    float r = s * 0.27f;
    fillEllipse(layer, cx - s * 0.23f, cy - s * 0.12f * vs, 2 * r, 2 * r * vs, col);
    fillEllipse(layer, cx + s * 0.23f, cy - s * 0.12f * vs, 2 * r, 2 * r * vs, col);
    fillTri(layer, cx - s * 0.49f, cy - s * 0.06f * vs, cx + s * 0.49f, cy - s * 0.06f * vs, cx,
            cy + s * 0.44f * vs, col);
    return;
  }

  float r = LV_MIN(eye_.r.x * scale, LV_MIN(w, hv) / 2);
  fillRect(layer, x1, y1, x2, y2, r, col);

  // Bovenooglid: zwart vlak met schuine onderrand. Binnenhoek = kant richting schermmidden.
  float lid = LV_CLAMP(0.f, eye_.lidTop.x, 1.f);
  float tilt = eye_.tilt.x;
  if (lid > 0.005f || fabsf(tilt) > 0.5f) {
    float slope = tanf(tilt * kPi / 180.0f) * (w / 2 + 8);
    float base = y1 + lid * hv;
    float yInner = base + slope, yOuter = base - slope;
    float yl = (side < 0) ? yOuter : yInner;  // links-oog: binnenkant is rechts
    float yr = (side < 0) ? yInner : yOuter;
    float top = y1 - 12;
    yl = LV_MAX(yl, top);
    yr = LV_MAX(yr, top);
    // Rechthoek tot het hoogste punt + één driehoek voor de schuine wig. Geen naad tussen twee
    // driehoeken door het oog heen (die lekt door anti-aliasing een lijntje oogkleur).
    float ymin = LV_MIN(yl, yr);
    float xl = x1 - 8, xr = x2 + 8;
    fillRect(layer, xl, top, xr, ymin + 2, 0, kBlack);
    if (yl < yr) fillTri(layer, xl, yl, xr, yl, xr, yr, kBlack);
    else if (yr < yl) fillTri(layer, xl, yr, xr, yr, xl, yl, kBlack);
  }

  // Onderooglid: zwarte ellips die van onderen omhoog komt -> blije "^"-vorm.
  float lidB = LV_CLAMP(0.f, eye_.lidBottom.x, 1.f);
  if (lidB > 0.005f) {
    float ew = w * 1.7f, eh = hv * 1.3f;
    float topY = y2 - lidB * hv;
    fillEllipse(layer, cx, topY + eh / 2, ew, eh, kBlack);
  }
}

void Face::drawMouth(lv_layer_t* layer, float cx, float cy) {
  lv_color_t col = lv_color_make((uint8_t)colR_.x, (uint8_t)colG_.x, (uint8_t)colB_.x);
  lv_opa_t opa = (lv_opa_t)(LV_CLAMP(0.f, mouthAlpha_, 1.f) * 255);
  lv_draw_arc_dsc_t a;
  lv_draw_arc_dsc_init(&a);
  a.color = col;
  a.opa = opa;
  a.width = 9;
  a.rounded = 1;
  switch (mouthShown_) {
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

void Face::drawZs(lv_layer_t* layer, lv_color_t color) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.color = color;
  d.text = "z";
  for (int i = 0; i < 3; i++) {
    float p = fmodf(t_ * 0.33f + i / 3.0f, 1.0f);
    float x = kCenter + 110 + p * 50 + sinf(p * 6) * 6;
    float y = kCenter - 70 - p * 120;
    d.font = (p < 0.33f) ? &lv_font_montserrat_20 : (p < 0.66f ? &lv_font_montserrat_28 : &buddy_48);
    d.opa = (lv_opa_t)(sinf(p * kPi) * 230);
    lv_area_t a = {ri(x), ri(y), ri(x) + 50, ri(y) + 60};
    lv_draw_label(layer, &d, &a);
  }
}

}  // namespace buddy
