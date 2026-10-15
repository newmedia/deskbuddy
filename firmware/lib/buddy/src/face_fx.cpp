// Acties (fluiten, knipoog, kusje, rage, ontploffen) en effecten: deeltjes, trillen, tint, flits en
// roet. Werkt over beide skins heen; de skins lezen alleen wink_/pucker_/swell_ uit.

#include <math.h>
#include <string.h>

#include "draw_util.h"
#include "face.h"

namespace buddy {

using namespace draw;

namespace {

constexpr float C = Face::kSize / 2.0f;
constexpr float kPi = 3.14159265f;
const char* const kActionNames[] = {"none", "whistle", "wink", "kiss", "rage", "explode"};

// Duur per actie in seconden
float durationOf(Action a) {
  switch (a) {
    case Action::Whistle: return 3.4f;
    case Action::Wink: return 1.0f;
    case Action::Kiss: return 2.6f;
    case Action::Rage: return 4.0f;
    case Action::Explode: return 6.8f;
    default: return 0;
  }
}

}  // namespace

const char* actionName(Action a) {
  unsigned i = (unsigned)a;
  return i < (unsigned)Action::Count ? kActionNames[i] : "?";
}

bool actionFromName(const char* name, Action& out) {
  for (unsigned i = 1; i < (unsigned)Action::Count; i++)
    if (!strcmp(name, kActionNames[i])) {
      out = (Action)i;
      return true;
    }
  return false;
}

void Face::play(Action a) {
  if (a == Action::None) {
    actT_ = durationOf(action_);  // laat updateAction netjes afronden
    return;
  }
  if (!busy()) actPrevEmotion_ = emotion_;
  action_ = a;
  actT_ = 0;
  actNext_ = 0;
  actStage_ = 0;
}

float Face::eyeClosure(int side) const {
  float blink = blinkT_ < 0 ? 0 : sinf(blinkT_ * kPi);
  return LV_MAX(blink, wink_[side > 0 ? 1 : 0]);
}

bool Face::effectsActive() const {
  return busy() || particleCount_ > 0 || soot_ > 0.01f || tint_ > 0.01f || flash_ > 0.01f || shake_ > 0.1f;
}

void Face::spawn(uint8_t type, float x, float y, float vx, float vy, float life, float size, uint32_t color) {
  if (particleCount_ >= kMaxParticles) return;
  Particle& p = particles_[particleCount_++];
  p = {x, y, vx, vy, life, life, size, rnd() * 6.28f, type, color};
}

void Face::updateParticles(float dt) {
  for (int i = 0; i < particleCount_;) {
    Particle& p = particles_[i];
    p.life -= dt;
    if (p.life <= 0) {
      particles_[i] = particles_[--particleCount_];
      continue;
    }
    if (p.type == PNote) p.vx = 20 + sinf(p.life * 6 + p.spin) * 45;  // zwalkend omhoog
    if (p.type == PSpark) p.vy += 620 * dt;                          // zwaartekracht
    if (p.type == PSteam || p.type == PSmoke) {
      p.vx *= 1 - dt * 1.5f;
      p.vy *= 1 - dt * 0.8f;
    }
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    i++;
  }
}

// Positie van de mond en de ogen per skin (voor het startpunt van deeltjes)
static void mouthPos(Skin s, float& x, float& y) {
  x = C;
  y = s == Skin::Mascot ? 350 : C + 112;
}
static void eyePos(Skin s, int side, float& x, float& y) {
  x = C + side * (s == Skin::Mascot ? 82 : 94);
  y = s == Skin::Mascot ? 246 : C - 18;
}

void Face::updateAction(float dt) {
  // Effecten die na een actie nog uitdoven
  flash_ = LV_MAX(0.0f, flash_ - dt * 2.2f);
  if (!busy()) {
    shake_ = LV_MAX(0.0f, shake_ - dt * 20);
    tint_ = LV_MAX(0.0f, tint_ - dt * 2);
    soot_ = LV_MAX(0.0f, soot_ - dt * 0.6f);
    return;
  }

  const bool mascot = skin_ == Skin::Mascot;
  const uint32_t ink = mascot ? 0x0B2290 : 0x6EE7F7;  // kleur van noten e.d., goed zichtbaar per skin
  float t = actT_;
  float mx, my;
  mouthPos(skin_, mx, my);

  switch (action_) {
    case Action::Whistle:
      if (actStage_ == 0) {
        actStage_ = 1;
        cue_ = Cue::Whistle;
        pucker_ = true;
        eye_.lidTop.target = 0.34f;  // ontspannen halfdichte ogen
        lookAt(0.55f, -0.6f, true);
      }
      if (t >= actNext_ && t < 2.9f) {
        actNext_ = t + 0.38f;
        spawn(PNote, mx + 18, my - 8, 20, -70, 1.7f, 16 + rnd() * 6, ink);
      }
      break;

    case Action::Wink:
      if (actStage_ == 0) {
        actStage_ = 1;
        cue_ = Cue::Wink;
        blush_.target = LV_MAX(blush_.target, 0.4f);
      }
      wink_[1] = t < 0.1f ? t / 0.1f : (t < 0.6f ? 1.0f : LV_MAX(0.0f, 1 - (t - 0.6f) / 0.15f));
      if (actStage_ == 1 && t > 0.15f) {
        actStage_ = 2;
        float ex, ey;
        eyePos(skin_, 1, ex, ey);
        spawn(PStar, ex + 58, ey - 52, 0, -20, 0.7f, 26, mascot ? 0xFFD54A : 0xFFFFFF);
      }
      break;

    case Action::Kiss:
      if (actStage_ == 0) {
        actStage_ = 1;
        pucker_ = true;
        blush_.target = 0.75f;
      }
      wink_[0] = wink_[1] = (t > 0.1f && t < 1.7f) ? LV_MIN(1.0f, (t - 0.1f) / 0.12f) : LV_MAX(0.0f, wink_[0] - dt * 8);
      if (actStage_ == 1 && t > 0.9f) {
        actStage_ = 2;
        cue_ = Cue::Kiss;
        spawn(PHeart, mx, my - 10, 0, -95, 1.5f, 34, 0xFF4F8B);
      }
      if (t > 1.9f) pucker_ = false;
      break;

    case Action::Rage:
      if (actStage_ == 0) {
        actStage_ = 1;
        cue_ = Cue::Rage;
        setEmotion(Emotion::Angry);
      }
      tint_ = 0.32f + 0.12f * sinf(t * 14);
      shake_ = 5.5f;
      if (t >= actNext_ && t < 3.6f) {
        actNext_ = t + 0.16f;
        for (int side = -1; side <= 1; side += 2) {
          // Stoom uit de "oren": classic naast de ogen, mascotte naast de bril
          float sx = mascot ? C + side * 168 : C + side * 158, sy = mascot ? 300 : C - 30;
          spawn(PSteam, sx, sy, side * (55 + rnd() * 45), -70 - rnd() * 50, 1.0f, 30 + rnd() * 8, 0xF0F0F0);
        }
        if (rnd() < 0.4f)
          spawn(PSymbol, C - 90 + rnd() * 140, mascot ? 150 : 90 + rnd() * 30, 0, -35, 0.9f, 1,
                rnd() < 0.5f ? 0xFF3B30 : 0xFFD54A);
      }
      break;

    case Action::Explode:
      if (actStage_ == 0) {
        actStage_ = 1;
        cue_ = Cue::Fuse;
        setEmotion(Emotion::Angry);
      }
      if (t < 1.4f) {
        float k = t / 1.4f;
        swell_ = 1 + 0.38f * k * k;
        shake_ = 2 + 9 * k;
        tint_ = 0.65f * k;
      } else if (actStage_ == 1) {
        actStage_ = 2;
        cue_ = Cue::Boom;
        flash_ = 1;
        swell_ = 1;
        tint_ = 0;
        shake_ = 13;
        soot_ = 1;
        const uint32_t cols[] = {ink, 0xFFB347, 0xFF5A4E, 0xFFE066, 0xFFFFFF};
        for (int i = 0; i < 30; i++) {
          float a = rnd() * 2 * kPi, sp = 180 + rnd() * 260;
          spawn(PSpark, C, C, cosf(a) * sp, sinf(a) * sp - 120, 0.8f + rnd() * 0.7f, 6 + rnd() * 9, cols[i % 5]);
        }
        setEmotion(Emotion::Dizzy);
      } else {
        shake_ = LV_MAX(0.0f, shake_ - dt * 12);
        if (t >= actNext_ && t < 5.6f) {
          actNext_ = t + 0.28f;
          spawn(PSmoke, C - 120 + rnd() * 240, mascot ? 120 : 150, (rnd() - 0.5f) * 30, -45, 1.7f, 26,
                mascot ? 0x3A3A3A : 0x7A7A7A);
        }
        if (actStage_ == 2 && t > 3.6f) {
          actStage_ = 3;
          setEmotion(Emotion::Surprised);
        }
        if (t > 5.2f) soot_ = LV_MAX(0.0f, 1 - (t - 5.2f) / 1.6f);
      }
      break;

    default:
      break;
  }

  actT_ += dt;
  if (actT_ >= durationOf(action_)) {
    // Afronden: alles terug, vorige emotie (geforceerd, ook als die gelijk is aan de huidige)
    action_ = Action::None;
    pucker_ = false;
    wink_[0] = wink_[1] = 0;
    swell_ = 1;
    releaseLook();
    emotion_ = actPrevEmotion_;
    applyPose(poseFor(emotion_));
    mascotTargets();
  }
}

void Face::drawPucker(lv_layer_t* layer, float cx, float cy, lv_color_t col, bool mascot) {
  if (mascot) {
    fillEllipse(layer, cx, cy, 46, 40, lv_color_hex(0x111522));
    fillEllipse(layer, cx, cy, 38, 32, lv_color_hex(0xD86A7A));
    fillEllipse(layer, cx, cy, 16, 13, lv_color_hex(0x5A0F1C));
  } else {
    lv_draw_arc_dsc_t a;
    lv_draw_arc_dsc_init(&a);
    a.color = col;
    a.width = 8;
    a.center = {ri(cx), ri(cy)};
    a.radius = 13;
    a.start_angle = 0;
    a.end_angle = 360;
    lv_draw_arc(layer, &a);
  }
}

void Face::drawEffects(lv_layer_t* layer) {
  const bool mascot = skin_ == Skin::Mascot;

  // Roet (na het ontploffen)
  if (soot_ > 0.01f) {
    lv_opa_t o = (lv_opa_t)(soot_ * (mascot ? 210 : 235));
    lv_color_t sc = lv_color_hex(mascot ? 0x2B2522 : 0x3C3C3C);
    float ex, ey;
    eyePos(skin_, -1, ex, ey);
    fillEllipse(layer, ex + 18, ey - 14, 70, 34, sc, o);
    fillEllipse(layer, ex - 40, ey + 66, 46, 26, sc, o);
    eyePos(skin_, 1, ex, ey);
    fillEllipse(layer, ex - 10, ey + 26, 52, 30, sc, o);
    fillEllipse(layer, ex + 44, ey - 70, 38, 22, sc, o);
    fillEllipse(layer, C + 30, mascot ? 410 : C + 150, 40, 22, sc, o);
  }

  for (int i = 0; i < particleCount_; i++) {
    const Particle& p = particles_[i];
    float f = p.life / p.maxLife;  // 1 -> 0
    lv_opa_t o = (lv_opa_t)(LV_MIN(1.0f, f * 2.5f) * 255);
    lv_color_t c = lv_color_hex(p.color);
    switch (p.type) {
      case PNote: {
        float s = p.size;
        fillEllipse(layer, p.x, p.y, s, s * 0.76f, c, o);
        line(layer, p.x + s * 0.42f, p.y, p.x + s * 0.42f, p.y - s * 1.7f, 3.5f, c, o);
        line(layer, p.x + s * 0.42f, p.y - s * 1.7f, p.x + s * 1.05f, p.y - s * 1.15f, 3.5f, c, o);
        break;
      }
      case PHeart:
        heart(layer, p.x, p.y, p.size * (1 + (1 - f) * 1.8f), c);
        break;
      case PSteam:
      case PSmoke: {
        float s = p.size * (1 + (1 - f) * 1.6f);
        fillEllipse(layer, p.x, p.y, s, s * 0.85f, c, (lv_opa_t)(f * (p.type == PSteam ? 170 : 200)));
        break;
      }
      case PSpark:
        fillEllipse(layer, p.x, p.y, p.size, p.size, c, o);
        break;
      case PSymbol: {
        static const char* const sym[] = {"#", "@", "!", "$", "%", "&"};
        lv_draw_label_dsc_t d;
        lv_draw_label_dsc_init(&d);
        d.color = c;
        d.opa = o;
        d.font = &buddy_48;
        d.text = sym[(int)(p.spin) % 6];
        lv_area_t a = {ri(p.x), ri(p.y), ri(p.x) + 50, ri(p.y) + 60};
        lv_draw_label(layer, &d, &a);
        break;
      }
      case PStar: {
        float s = p.size * (0.6f + 0.4f * sinf(f * kPi));
        line(layer, p.x - s, p.y, p.x + s, p.y, 5, c, o);
        line(layer, p.x, p.y - s, p.x, p.y + s, 5, c, o);
        line(layer, p.x - s * 0.5f, p.y - s * 0.5f, p.x + s * 0.5f, p.y + s * 0.5f, 3, c, o);
        line(layer, p.x - s * 0.5f, p.y + s * 0.5f, p.x + s * 0.5f, p.y - s * 0.5f, 3, c, o);
        break;
      }
      default:
        break;
    }
  }

  if (tint_ > 0.01f) fillRect(layer, 0, 0, kSize, kSize, 0, lv_color_hex(0xFF1E1E), (lv_opa_t)(tint_ * 110));
  if (flash_ > 0.01f) fillRect(layer, 0, 0, kSize, kSize, 0, lv_color_white(), (lv_opa_t)(LV_MIN(1.0f, flash_) * 255));
}

}  // namespace buddy
