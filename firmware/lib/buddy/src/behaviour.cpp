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

  // Tapping adds up (1.2 s window between taps): 5x = angry, 9x = rage, 4 more during rage = boom.
  if (g.type == Gesture::Tap || g.type == Gesture::DoubleTap) {
    taps_ += g.type == Gesture::DoubleTap ? 2 : 1;
    tapWindow_ = 1200;
  }
  if (face_.busy()) {
    if (face_.action() == Action::Rage && (g.type == Gesture::Tap || g.type == Gesture::DoubleTap) && taps_ >= 13) {
      taps_ = 0;
      face_.play(Action::Explode);
    }
    return;  // nothing else during an action
  }
  if (taps_ >= 9 && face_.emotion() == Emotion::Angry) {
    face_.play(Action::Rage);
    return;
  }
  if (taps_ >= 5 && (g.type == Gesture::Tap || g.type == Gesture::DoubleTap)) {
    react(Emotion::Angry, 2500);
    return;
  }
  if (g.type == Gesture::Shake && face_.emotion() == Emotion::Angry) {
    face_.play(Action::Rage);
    return;
  }

  switch (g.type) {
    case Gesture::Tap:
      face_.lookAt(g.x, g.y, false);
      face_.bounce(0.6f);
      if (sleeping) react(Emotion::Surprised, 900);
      else if (!manual_) react(rnd() < 0.5f ? Emotion::Happy : Emotion::Surprised, 1100);
      break;
    case Gesture::DoubleTap:
      face_.play(Action::Wink);
      break;
    case Gesture::LongPress:
      react(Emotion::Love, 3500);
      break;
    case Gesture::Pet:
      if (sleeping) {
        wake();
        break;
      }
      // Tickling: 3x petting within 2.5 s = laugh
      if (quickPetWindow_ == 0) quickPets_ = 0;
      quickPetWindow_ = 2500;
      if (++quickPets_ >= 3) {
        quickPets_ = 0;
        face_.play(Action::Laugh);
        break;
      }
      if (petWindow_ == 0) pets_ = 0;
      petWindow_ = 6000;
      ++pets_;
      if (pets_ >= 5) {
        pets_ = 0;
        face_.play(Action::Kiss);
      } else if (pets_ >= 3) {
        react(Emotion::Love, 4000);
      } else {
        react(Emotion::Happy, 2500);
      }
      break;
    case Gesture::SwipeLeft:
    case Gesture::SwipeRight:
      if (swipeWindow_) {  // two quick back-and-forth swipes = roll the eyes
        swipeWindow_ = 0;
        face_.play(Action::EyeRoll);
        break;
      }
      swipeWindow_ = 1200;
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

void Behaviour::onSound(SoundEvent e) {
  if (e == SoundEvent::None) return;
  bool sleeping = face_.emotion() == Emotion::Asleep || face_.emotion() == Emotion::Sleepy;
  switch (e) {
    case SoundEvent::Clap:  // startle, or wake up
      idleMs_ = 0;
      if (face_.busy() || dancing_) break;
      react(Emotion::Surprised, 1000, sleeping ? Emotion::Neutral : Emotion::Happy);
      face_.bounce(0.9f);
      break;
    case SoundEvent::DoubleClap:  // happy little hop
      idleMs_ = 0;
      if (face_.busy()) break;
      react(Emotion::Happy, 1500);
      face_.hop(36);
      break;
    case SoundEvent::Loud:  // lots of noise: squeeze the eyes shut
      idleMs_ = 0;
      if (face_.busy() || dancing_) break;
      react(Emotion::Pain, 2200, Emotion::Neutral);
      break;
    case SoundEvent::SpeechStart:  // someone is talking: prick up the ears (alternately left and right)
      if (!sleeping && !dancing_) face_.setListening(true);
      break;
    case SoundEvent::SpeechStop:
      face_.setListening(false);
      break;
    case SoundEvent::MusicStart:
      face_.setListening(false);
      idleMs_ = 0;
      dancing_ = true;
      if (!face_.busy()) react(Emotion::Happy, 0);
      break;
    case SoundEvent::MusicStop:
      if (!dancing_) break;
      dancing_ = false;
      face_.releaseLook();
      if (!face_.busy()) react(Emotion::Happy, 1500);
      break;
    case SoundEvent::Beat:  // on the beat: little hop and look alternately to the sides
      idleMs_ = 0;
      if (!dancing_ || face_.busy()) break;
      if (face_.emotion() != Emotion::Happy && face_.emotion() != Emotion::Love) react(Emotion::Happy, 0);
      face_.hop(12 + rnd() * 10);
      beatSide_ = -beatSide_;
      face_.lookAt(0.55f * beatSide_, -0.2f, true);
      break;
    default:
      break;
  }
}

void Behaviour::onGravity(float gx, float gy, float gz) {
  (void)gz;
  float tx = gx * tiltSignX, ty = gy * tiltSignY;
  bool tilted = fabsf(tx) > 0.17f || fabsf(ty) > 0.17f;  // ~10°
  if (tilted && !asleep()) {
    // Look "downhill", as if the eyes roll along.
    face_.lookAt(LV_CLAMP(-1.f, tx * 2.2f, 1.f), LV_CLAMP(-1.f, ty * 2.2f, 1.f), true);
    tiltLook_ = true;
  } else if (tiltLook_) {
    face_.releaseLook();
    tiltLook_ = false;
  }
}

void Behaviour::update(uint32_t dtMs) {
  idleMs_ += dtMs;
  if (petWindow_) petWindow_ = dtMs >= petWindow_ ? 0 : petWindow_ - dtMs;
  if (quickPetWindow_) quickPetWindow_ = dtMs >= quickPetWindow_ ? 0 : quickPetWindow_ - dtMs;
  if (swipeWindow_) swipeWindow_ = dtMs >= swipeWindow_ ? 0 : swipeWindow_ - dtMs;
  if (tapWindow_) {
    tapWindow_ = dtMs >= tapWindow_ ? 0 : tapWindow_ - dtMs;
    if (!tapWindow_) taps_ = 0;
  }
  if (face_.busy()) return;  // actions handle their own emotion and timing

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

  // Boredom -> sleepy -> sleep
  if (idleMs_ >= asleepMs_) {
    face_.setEmotion(Emotion::Asleep);
  } else if (idleMs_ >= sleepyMs_) {
    face_.setEmotion(Emotion::Sleepy);
  } else if (idleMs_ >= nextIdleQuirk_) {
    // Occasionally a small spontaneous expression, so it doesn't feel like a screensaver.
    float r = rnd();
    if (r < 0.25f) face_.play(Action::Whistle);  // occasionally a little tune
    else if (r < 0.33f) face_.play(Action::Sneeze);
    else if (r < 0.40f) face_.play(Action::Hiccup);
    else react(r < 0.65f ? Emotion::Happy : (r < 0.85f ? Emotion::Surprised : Emotion::Love), 1200);
    nextIdleQuirk_ = idleMs_ + 20000 + (uint32_t)(rnd() * 25000);
  }
  if (idleMs_ < nextIdleQuirk_ - 60000) nextIdleQuirk_ = idleMs_ + 15000;
}

}  // namespace buddy
