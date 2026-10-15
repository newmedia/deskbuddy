#include "behaviour.h"

#include <math.h>

namespace buddy {

float Behaviour::rnd() {
  seed_ = seed_ * 1664525u + 1013904223u;
  return (seed_ >> 8) / 16777216.0f;
}

void Behaviour::react(Emotion e, uint32_t ms, Emotion then) {
  face_.setEmotion(e);
  holdLeft_ = ms;
  holding_ = ms > 0;
  then_ = then;
  manual_ = false;
}

void Behaviour::wake() {
  bool wasSleeping = face_.emotion() == Emotion::Asleep || face_.emotion() == Emotion::Sleepy;
  idleMs_ = 0;
  if (wasSleeping) react(Emotion::Surprised, 900);
}

void Behaviour::command(Emotion e, uint32_t holdMs) {
  idleMs_ = 0;
  if (holdMs) {
    react(e, holdMs);
  } else {
    react(e, 0);
    manual_ = e != Emotion::Neutral;
  }
}

void Behaviour::onGesture(const GestureEvent& g) {
  if (g.type == Gesture::None) return;
  bool sleeping = face_.emotion() == Emotion::Asleep || face_.emotion() == Emotion::Sleepy;
  idleMs_ = 0;

  switch (g.type) {
    case Gesture::Tap:
      face_.lookAt(g.x, g.y, false);
      face_.bounce(0.6f);
      if (sleeping) react(Emotion::Surprised, 900);
      else if (!manual_) react(rnd() < 0.5f ? Emotion::Happy : Emotion::Surprised, 1100);
      break;
    case Gesture::DoubleTap:
      face_.bounce(1.0f);
      react(Emotion::Happy, 2200);
      break;
    case Gesture::LongPress:
      react(Emotion::Love, 3500);
      break;
    case Gesture::Pet:
      if (sleeping) {
        wake();
        break;
      }
      if (petWindow_ == 0) pets_ = 0;
      petWindow_ = 6000;
      if (++pets_ >= 3) react(Emotion::Love, 4000);
      else react(Emotion::Happy, 2500);
      break;
    case Gesture::SwipeLeft:
    case Gesture::SwipeRight:
      face_.lookAt(g.type == Gesture::SwipeLeft ? -1.0f : 1.0f, 0, false);
      if (sleeping) wake();
      break;
    case Gesture::Shake:
      react(Emotion::Dizzy, 2800, Emotion::Angry);
      break;
    case Gesture::PickUp:
      if (face_.emotion() != Emotion::Dizzy) react(Emotion::Surprised, 1500, sleeping ? Emotion::Neutral : Emotion::Happy);
      break;
    default:
      if (sleeping) wake();
      break;
  }
}

void Behaviour::onGravity(float gx, float gy, float gz) {
  (void)gz;
  float tx = gx * tiltSignX, ty = gy * tiltSignY;
  bool tilted = fabsf(tx) > 0.28f || fabsf(ty) > 0.28f;
  if (tilted && !asleep()) {
    // Kijk "bergafwaarts", net alsof de ogen meerollen.
    face_.lookAt(LV_CLAMP(-1.f, tx * 1.6f, 1.f), LV_CLAMP(-1.f, ty * 1.6f, 1.f), true);
    tiltLook_ = true;
  } else if (tiltLook_) {
    face_.releaseLook();
    tiltLook_ = false;
  }
}

void Behaviour::update(uint32_t dtMs) {
  idleMs_ += dtMs;
  if (petWindow_) petWindow_ = dtMs >= petWindow_ ? 0 : petWindow_ - dtMs;

  if (holding_) {
    if (dtMs >= holdLeft_) {
      holding_ = false;
      Emotion next = then_;
      then_ = Emotion::Neutral;
      if (next == Emotion::Angry) react(Emotion::Angry, 1600);
      else if (next == Emotion::Happy) react(Emotion::Happy, 1500);
      else face_.setEmotion(next);
    } else {
      holdLeft_ -= dtMs;
    }
    return;
  }
  if (manual_) return;

  // Verveling -> slaperig -> slapen
  if (idleMs_ >= asleepMs_) {
    face_.setEmotion(Emotion::Asleep);
  } else if (idleMs_ >= sleepyMs_) {
    face_.setEmotion(Emotion::Sleepy);
  } else if (idleMs_ >= nextIdleQuirk_) {
    // Af en toe een kleine spontane expressie, zodat hij niet als een screensaver voelt.
    float r = rnd();
    react(r < 0.5f ? Emotion::Happy : (r < 0.8f ? Emotion::Surprised : Emotion::Love), 1200);
    nextIdleQuirk_ = idleMs_ + 20000 + (uint32_t)(rnd() * 25000);
  }
  if (idleMs_ < nextIdleQuirk_ - 60000) nextIdleQuirk_ = idleMs_ + 15000;
}

}  // namespace buddy
