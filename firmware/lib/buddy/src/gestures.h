#pragma once
// Herkent gebaren uit ruwe touch- en accelerometer-samples. Platform-onafhankelijk.

#include <stdint.h>

namespace buddy {

enum class Gesture : uint8_t {
  None,
  Tap,
  DoubleTap,
  LongPress,
  Pet,         // heen-en-weer aaien
  SwipeLeft,
  SwipeRight,
  SwipeUp,
  SwipeDown,
  Shake,
  PickUp,
};

const char* gestureName(Gesture g);

struct GestureEvent {
  Gesture type = Gesture::None;
  float x = 0, y = 0;  // schermpositie (touch), genormaliseerd -1..1
};

class TouchGestures {
 public:
  // Aanroepen bij elke touch-poll (ook als niet ingedrukt). Geeft hooguit één event terug.
  GestureEvent feed(bool pressed, int x, int y, uint32_t nowMs);

 private:
  bool down_ = false;
  bool longFired_ = false;
  bool moved_ = false;
  int sx_ = 0, sy_ = 0, lx_ = 0, ly_ = 0;
  uint32_t t0_ = 0, lastTap_ = 0;
  float path_ = 0;
  int lastDirX_ = 0, reversals_ = 0;
};

class MotionGestures {
 public:
  // Acceleratie in g. Geeft hooguit één event terug.
  GestureEvent feed(float ax, float ay, float az, uint32_t nowMs);
  // Laagdoorlaat-zwaartekrachtvector, voor "kijk naar beneden als je hem kantelt".
  float gx() const { return gx_; }
  float gy() const { return gy_; }
  float gz() const { return gz_; }

 private:
  float gx_ = 0, gy_ = 0, gz_ = 1;
  float still_ = 0;          // hoe lang (ms) het apparaat stil ligt
  uint32_t last_ = 0;
  uint32_t shakeWindow_ = 0, lastShake_ = 0, lastPick_ = 0;
  int shakeHits_ = 0;
};

}  // namespace buddy
