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
                              "sleepy",  "love",  "dizzy", "asleep", "cry", "hyper", "pain"};

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
  look_.base = (uint8_t)s;
  fullRedraw_ = true;
  if (obj_) lv_obj_invalidate(obj_);
}

void Face::setLook(const Look& l) {
  look_ = l;
  skin_ = l.base ? Skin::Mascot : Skin::Classic;
  applyPose(poseFor(emotion_));  // kleuren opnieuw bepalen
  mascotTargets();
  fullRedraw_ = true;
  if (obj_) lv_obj_invalidate(obj_);
}

// Kleur van de robotogen: de eigen hoofdkleur, behalve bij sterke emoties (als emotiekleuren aan staan).
uint32_t Face::poseColor(Emotion e, uint32_t emotionColor) const {
  if (look_.emotionColors &&
      (e == Emotion::Angry || e == Emotion::Sad || e == Emotion::Love || e == Emotion::Dizzy ||
       e == Emotion::Cry || e == Emotion::Pain))
    return emotionColor;
  uint32_t c = look_.primary;
  if (e == Emotion::Asleep) {  // gedimd
    c = (((c >> 16) & 0xff) * 45 / 100) << 16 | (((c >> 8) & 0xff) * 45 / 100) << 8 | ((c & 0xff) * 45 / 100);
  }
  return c;
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
    case Emotion::Cry:       return { 96, 110, 34, 0.36f, -18, 0.12f,  14, 0.00f, 0x5AA0FF, MouthFrown, false};
    case Emotion::Hyper:     return {124, 140, 46, 0.00f,   0, 0.00f,  -8, 0.35f, 0x6EE7F7, MouthSmile, false};
    case Emotion::Pain:      return {112, 112, 26, 0.40f, -20, 0.26f,   6, 0.00f, 0xFF8A4E, MouthFlat,  false};
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
  uint32_t c0 = poseColor(emotion_, pose_.color);
  colR_.snap((c0 >> 16) & 0xff);
  colG_.snap((c0 >> 8) & 0xff);
  colB_.snap(c0 & 0xff);
  for (Spring* s : {&colR_, &colG_, &colB_, &blush_}) {
    s->k = 60;
    s->d = 16;
  }
  lookX_.k = lookY_.k = 260;
  lookX_.d = lookY_.d = 27;  // nog een klein beetje naveren, maar sneller stil (minder hertekenen)
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
  fullRedraw_ = true;  // anders blijven resten staan buiten de deelgebieden (Z'jes, tranen, zweet)
  applyPose(poseFor(e));
  mascotTargets();
  if (e == Emotion::Surprised || e == Emotion::Happy || e == Emotion::Love || e == Emotion::Hyper) bounce(0.8f);
  if (e == Emotion::Pain) nextTwinge_ = t_;  // meteen een steek
  if (e == Emotion::Hyper) nextSaccade_ = t_;
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
  uint32_t c = poseColor(emotion_, p.color);
  colR_.target = (c >> 16) & 0xff;
  colG_.target = (c >> 8) & 0xff;
  colB_.target = c & 0xff;
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

void Face::hop(float height) {
  if (hopT_ >= 0) return;
  hopT_ = 0;
  hopH_ = height;
  // Tijdens de sprong één momentopname verschuiven i.p.v. elk frame het hele gezicht opnieuw te tekenen:
  // de cartoon kost ~60 ms per volledig frame, een plaatje kopiëren een fractie daarvan.
  if (obj_ && !hopSnap_ && !matrix_ && !covered_ && !effectsActive())
    hopSnap_ = lv_snapshot_take(obj_, LV_COLOR_FORMAT_RGB565);
  bounce(0.5f);  // eerst even inveren voor de afzet
}

float Face::rnd() {
  seed_ = seed_ * 1664525u + 1013904223u;
  return (seed_ >> 8) / 16777216.0f;
}

void Face::scheduleNextBlink() {
  float base = (emotion_ == Emotion::Sleepy) ? 1.6f : 2.2f;
  nextBlink_ = t_ + base + rnd() * 3.8f;
}

void Face::scheduleNextSaccade() {
  // Druk: de blik schiet om de paar tienden van een seconde ergens anders heen.
  nextSaccade_ = t_ + (emotion_ == Emotion::Hyper ? 0.12f + rnd() * 0.3f : 0.9f + rnd() * 2.8f);
}

void Face::update(uint32_t dtMs) {
  float dt = LV_MIN(dtMs, 50u) / 1000.0f;
  t_ += dt;
  if (matrix_) {
    for (int e = 0; e < 2; e++)
      for (int c = 0; c < 7; c++) {
        matrixHead_[e][c] += matrixSpeed_[e][c] * dt;
        if (matrixHead_[e][c] > 14) {  // onderaan uit beeld: opnieuw van boven
          matrixHead_[e][c] = -rnd() * 3;
          matrixSpeed_[e][c] = 5 + rnd() * 9;
        }
      }
  }

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
    if (emotion_ == Emotion::Hyper) {
      lookX_.target = (rnd() * 2 - 1) * kLookX;
      lookY_.target = (rnd() * 2 - 1) * 0.8f * kLookY;
    } else if (rnd() < 0.4f) {
      lookX_.target = lookY_.target = 0;
    } else {
      lookX_.target = (rnd() * 2 - 1) * 0.75f * kLookX;
      lookY_.target = (rnd() * 2 - 1) * 0.55f * kLookY;
    }
    scheduleNextSaccade();
  }

  // Druk: af en toe een sprongetje. Pijn: om de paar seconden een steek (trillen, ogen knijpen).
  if (emotion_ == Emotion::Hyper && !busy() && rnd() < dt * 0.6f) hop(14 + rnd() * 16);
  if (emotion_ == Emotion::Pain && !busy() && t_ >= nextTwinge_) {
    shake_ = 4.5f;
    eye_.lidTop.v += 4;
    eye_.h.v -= 200;
    nextTwinge_ = t_ + 1.8f + rnd() * 2.2f;
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
  bobPx_ = skin_ == Skin::Mascot ? 0 : (int)lroundf(sinf(t_ * 2 * kPi / 4.2f) * 3.0f);
  updateAction(dt);
  updateParticles(dt);
  // Trillen en stuiteren: de hele tekening verschuiven (draw::ox/oy). Bij een stuiter springt het
  // gezicht in een boog omhoog en veert het bij het landen na; de hik geeft een schok omhoog.
  float hopY = 0;
  if (hopT_ >= 0) {
    hopT_ += dt / 0.5f;
    if (hopT_ >= 1) {
      hopT_ = -1;
      bounce(0.9f);  // landen
    } else {
      hopY = -4 * hopT_ * (1 - hopT_) * hopH_;
    }
  }
  // (LV_CLAMP heeft geen buitenste haakjes: eerst apart uitrekenen)
  const float squashY = LV_CLAMP(-22.f, squash_.x * 30, 22.f);
  int32_t jx = 0, jy = (int32_t)lroundf(squashY + hopY);
  if (shake_ > 0.2f) {
    jx += (int32_t)((rnd() * 2 - 1) * shake_);
    jy += (int32_t)((rnd() * 2 - 1) * shake_);
  }
  if (jx != offX_ || jy != offY_) {
    offX_ = (int16_t)jx;
    offY_ = (int16_t)jy;
    fullRedraw_ = true;
  }
  // Sprong voorbij en weer op de plek: momentopname weg, het gezicht leeft weer
  if (hopSnap_ && (hopT_ < 0 || matrix_ || effectsActive()) && jx == 0 && jy == 0) {
    lv_draw_buf_destroy(hopSnap_);
    hopSnap_ = nullptr;
    fullRedraw_ = true;
  }

  squash_.target = 0;
  hairY_.target = -squash_.x * 8;  // haar veert alleen mee bij een stuiter

  invalidate();
}

static float closure(float blinkT) { return blinkT < 0 ? 0 : sinf(blinkT * kPi); }

void Face::setCovered(bool c) {
  if (c == covered_) return;
  covered_ = c;
  if (!c) fullRedraw_ = true;  // weer zichtbaar: één keer alles tekenen
}

bool Face::settled() {
  if (matrix_ || fullRedraw_ || effectsActive() || blinkT_ >= 0 || pendingHeart_ || hopT_ >= 0) return false;
  if (bobPx_ != drawnBobPx_) return false;
  if (look_.mouth == (uint8_t)MouthStyle::Equalizer && soundLevel_ > 0.02f) return false;
  // Emoties met een doorlopende animatie (hartslag, wiebelen, zwevende Z'jes, pulserende o-mond)
  if (emotion_ == Emotion::Love || emotion_ == Emotion::Dizzy || emotion_ == Emotion::Sleepy ||
      emotion_ == Emotion::Asleep || emotion_ == Emotion::Cry)
    return false;
  if (pose_.mouth != mouthShown_ || mouthShown_ == MouthO) return false;
  if (mouthShown_ != MouthNone && mouthAlpha_ < 1.0f) return false;
  bool s = true;
  for (Spring* sp : {&eye_.w, &eye_.h, &eye_.r, &lookX_, &lookY_, &dy_, &mouthWidth_, &hairY_}) s &= sp->settle(0.15f);
  for (Spring* sp : {&eye_.lidTop, &eye_.lidBottom, &blush_, &squash_, &mouthOpen_}) s &= sp->settle(0.003f);
  for (Spring* sp : {&eye_.tilt, &browAngle_, &browRaise_}) s &= sp->settle(0.05f);
  for (Spring* sp : {&colR_, &colG_, &colB_}) s &= sp->settle(0.6f);
  return s;
}

void Face::invalidate() {
  if (!obj_) return;
  // Stilstaand gezicht: het laatste frame klopt nog, dus niets hertekenen. Het eerste "stille" frame
  // tekenen we nog wel, zodat de exact vastgeklikte eindwaarden op het scherm staan.
  if (covered_) return;
  bool s = settled();
  if (s && wasSettled_) return;
  wasSettled_ = s;
  if (matrix_ && !fullRedraw_) {
    // Alleen de twee oogvensters veranderen: dat houdt de update-download snel.
    for (int side = -1; side <= 1; side += 2) {
      lv_area_t a = {(int32_t)(kCenter + side * 94 - 64), (int32_t)(kCenter - 18 - 56),
                     (int32_t)(kCenter + side * 94 + 64), (int32_t)(kCenter - 18 + 56)};
      lv_obj_invalidate_area(obj_, &a);
    }
    return;
  }
  if (skin_ == Skin::Mascot && !fullRedraw_ && !effectsActive() && fabsf(dy_.v) < 0.5f && fabsf(hairY_.v) < 0.5f &&
      fabsf(hairY_.x - hairY_.target) < 0.3f) {
    // Mascotte in rust: alleen ogen (in de bril), wenkbrauwen, mond en eventueel Z'jes hertekenen.
    float cy = 246 + dy_.x * 0.3f;
    for (int side = -1; side <= 1; side += 2) {
      float ex = kCenter + side * 82, bx = kCenter + side * 86, by = cy - 78 - browRaise_.x;
      lv_area_t eye = {(int32_t)(ex - 72), (int32_t)(cy - 58), (int32_t)(ex + 72), (int32_t)(cy + 56)};
      lv_area_t brow = {(int32_t)(bx - 64), (int32_t)(by - 40), (int32_t)(bx + 64), (int32_t)(by + 40)};
      lv_obj_invalidate_area(obj_, &eye);
      lv_obj_invalidate_area(obj_, &brow);
    }
    lv_area_t mouth = {(int32_t)(kCenter - 135), 288, (int32_t)(kCenter + 135), 448};
    lv_obj_invalidate_area(obj_, &mouth);
    if (emotion_ == Emotion::Cry) {  // tranen over de wangen
      for (int side = -1; side <= 1; side += 2) {
        float ex = kCenter + side * 82;
        lv_area_t tears = {(int32_t)(ex - 60), (int32_t)(cy + 10), (int32_t)(ex + 60), kSize - 1};
        lv_obj_invalidate_area(obj_, &tears);
      }
    }
    if (emotion_ == Emotion::Sleepy || emotion_ == Emotion::Asleep) {
      lv_area_t zs = {(int32_t)(kCenter + 100), 30, (int32_t)(kCenter + 215), (int32_t)(kCenter - 10)};
      lv_obj_invalidate_area(obj_, &zs);
    }
    prevBox_ = {0, 0, kSize - 1, kSize - 1};
    return;
  }
  if (skin_ == Skin::Mascot || fullRedraw_ || effectsActive()) {
    // Mascotte in beweging (stuiter, emotiewissel) of effecten: alles opnieuw.
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
    // wenkbrauwen, bril, vizier en monocle-kettinkje vallen buiten het oog zelf
    if (look_.brows || look_.glasses || look_.eyes == (uint8_t)EyeStyle::Visor)
      growBox(box, cx - maxW / 2 - 50, cy - maxH / 2 - 90, cx + maxW / 2 + 50, cy + maxH / 2 + 110);
  }
  if (look_.mouth != (uint8_t)MouthStyle::Auto) growBox(box, kCenter - 75, kCenter + 60, kCenter + 75, kCenter + 175);
  if (mouthShown_ != MouthNone || mouthAlpha_ > 0)
    growBox(box, kCenter - 60, kCenter + 60, kCenter + 60, kCenter + 160);
  if (emotion_ == Emotion::Cry)
    for (int side = -1; side <= 1; side += 2) {
      float cx = kCenter + side * kEyeSpacing + lx, cy = kCenter + kEyeY + dy_.x + ly;
      growBox(box, cx - 60, cy, cx + 60, cy + maxH / 2 + 190);
    }
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
  if (matrix_) {
    drawMatrix(layer);
    return;
  }
  if (hopSnap_) {
    // Sprongetje: de momentopname verschoven tekenen, de vrijgekomen strook in de achtergrondkleur
    const lv_color_t bg = lv_color_hex(look_.face);
    if (offY_ < 0) fillRect(layer, 0, kSize + offY_, kSize, kSize, 0, bg);
    if (offY_ > 0) fillRect(layer, 0, 0, kSize, offY_, 0, bg);
    if (offX_ < 0) fillRect(layer, kSize + offX_, 0, kSize, kSize, 0, bg);
    if (offX_ > 0) fillRect(layer, 0, 0, offX_, kSize, 0, bg);
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = hopSnap_;
    lv_area_t a = {offX_, offY_, offX_ + kSize - 1, offY_ + kSize - 1};
    lv_draw_image(layer, &d, &a);
    drawEffects(layer);
    return;
  }
  draw::ox = offX_;
  draw::oy = offY_;
  if (skin_ == Skin::Mascot) drawMascot(layer);
  else drawRobot(layer);
  draw::ox = draw::oy = 0;  // effecten (tint, flits, deeltjes) staan vast op het scherm
  drawEffects(layer);
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
    shift(a);
    lv_draw_label(layer, &d, &a);
  }
}

// Tranen: een plasje bij het ooglid en druppels die over de wang rollen (lus via t_).
void Face::drawTears(lv_layer_t* layer, float cx, float y0, int side) {
  const lv_color_t c = lv_color_hex(0x8FD3FF);
  fillEllipse(layer, cx + side * 8, y0 + 2, 40, 12, c, 210);
  for (int i = 0; i < 3; i++) {
    float p = fmodf(t_ * 0.85f + i / 3.0f + (side > 0 ? 0.17f : 0), 1.0f);
    float x = cx + side * (12 + p * 6), y = y0 + 8 + p * 150;
    float s = 7 + 4 * (1 - p);
    lv_opa_t o = (lv_opa_t)(p < 0.8f ? 235 : 235 * (1 - p) / 0.2f);
    if (p < 0.8f) fillTri(layer, x - s * 0.85f, y - s * 0.3f, x + s * 0.85f, y - s * 0.3f, x, y - s * 2.3f, c);
    fillEllipse(layer, x, y, 2 * s, 2.2f * s, c, o);
    fillEllipse(layer, x - s * 0.3f, y - s * 0.3f, s * 0.6f, s * 0.6f, lv_color_white(), o / 2);
  }
}

// Zweetdruppel (pijn)
void Face::drawSweat(lv_layer_t* layer, float x, float y) {
  const lv_color_t c = lv_color_hex(0x8FD3FF);
  fillTri(layer, x - 12, y - 4, x + 12, y - 4, x, y - 34, c);
  fillEllipse(layer, x, y + 4, 26, 28, c);
  fillEllipse(layer, x - 5, y, 8, 10, lv_color_white(), 200);
}

}  // namespace buddy
