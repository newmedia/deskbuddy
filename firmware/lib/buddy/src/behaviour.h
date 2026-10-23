#pragma once
// Behaviour: translates gestures, motion and time into emotions and gaze direction.

#include "face.h"
#include "gestures.h"
#include "mic.h"

namespace buddy {

class Behaviour {
 public:
  explicit Behaviour(Face& face) : face_(face) {}

  void onGesture(const GestureEvent& g);
  void onGravity(float gx, float gy, float gz);
  void onSound(SoundEvent e);  // from the microphone (see mic.h)
  bool dancing() const { return dancing_; }
  // Emotion from outside (serial, later web/BLE). holdMs = 0 -> stays until the next interaction.
  void command(Emotion e, uint32_t holdMs = 0);
  void update(uint32_t dtMs);

  void setSleepAfter(uint32_t sleepyMs, uint32_t asleepMs) {
    sleepyMs_ = sleepyMs;
    asleepMs_ = asleepMs;
  }
  bool asleep() const { return face_.emotion() == Emotion::Asleep; }
  uint32_t idleMs() const { return idleMs_; }

  // To set the tilt axes correctly on the real device.
  float tiltSignX = 1.0f, tiltSignY = 1.0f;

 private:
  void react(Emotion e, uint32_t ms, Emotion then = Emotion::Neutral);
  void wake();
  float rnd();

  Face& face_;
  uint32_t holdLeft_ = 0;
  bool holding_ = false;
  bool manual_ = false;         // emotion set via command without timeout
  Emotion then_ = Emotion::Neutral;
  uint32_t idleMs_ = 0;
  uint32_t sleepyMs_ = 90000, asleepMs_ = 180000;
  uint32_t nextIdleQuirk_ = 15000;
  uint32_t petWindow_ = 0;
  int pets_ = 0;
  bool tiltLook_ = false;
  // Getting angry from repeated tapping: angry -> rage -> explode
  int taps_ = 0;
  uint32_t tapWindow_ = 0;
  // Tickling (fast petting) and back-and-forth swiping
  int quickPets_ = 0;
  uint32_t quickPetWindow_ = 0;
  uint32_t swipeWindow_ = 0;
  uint32_t seed_ = 0xBEEF;
  // Dancing to music
  bool dancing_ = false;
  int beatSide_ = 1;
};

}  // namespace buddy
