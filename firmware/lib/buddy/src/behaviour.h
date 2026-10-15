#pragma once
// Gedrag: vertaalt gebaren, beweging en tijd naar emoties en blikrichting.

#include "face.h"
#include "gestures.h"

namespace buddy {

class Behaviour {
 public:
  explicit Behaviour(Face& face) : face_(face) {}

  void onGesture(const GestureEvent& g);
  void onGravity(float gx, float gy, float gz);
  // Emotie van buitenaf (serial, later web/BLE). holdMs = 0 -> blijft tot de volgende interactie.
  void command(Emotion e, uint32_t holdMs = 0);
  void update(uint32_t dtMs);

  void setSleepAfter(uint32_t sleepyMs, uint32_t asleepMs) {
    sleepyMs_ = sleepyMs;
    asleepMs_ = asleepMs;
  }
  bool asleep() const { return face_.emotion() == Emotion::Asleep; }
  uint32_t idleMs() const { return idleMs_; }

  // Om de kantel-assen op het echte apparaat goed te zetten.
  float tiltSignX = 1.0f, tiltSignY = 1.0f;

 private:
  void react(Emotion e, uint32_t ms, Emotion then = Emotion::Neutral);
  void wake();
  float rnd();

  Face& face_;
  uint32_t holdLeft_ = 0;
  bool holding_ = false;
  bool manual_ = false;         // emotie gezet via commando zonder timeout
  Emotion then_ = Emotion::Neutral;
  uint32_t idleMs_ = 0;
  uint32_t sleepyMs_ = 90000, asleepMs_ = 180000;
  uint32_t nextIdleQuirk_ = 15000;
  uint32_t petWindow_ = 0;
  int pets_ = 0;
  bool tiltLook_ = false;
  uint32_t seed_ = 0xBEEF;
};

}  // namespace buddy
