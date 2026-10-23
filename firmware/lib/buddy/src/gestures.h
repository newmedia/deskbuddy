#pragma once
// Recognises gestures from raw touch and accelerometer samples. Platform-independent.

#include <math.h>
#include <stdint.h>

namespace buddy {

enum class Gesture : uint8_t {
  None,
  Tap,
  DoubleTap,
  LongPress,
  Pet,         // back-and-forth petting
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
  float x = 0, y = 0;  // screen position (touch), normalised -1..1
};

class TouchGestures {
 public:
  // Call on every touch poll (also when not pressed). Returns at most one event.
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
  // Acceleration in g. Returns at most one event.
  GestureEvent feed(float ax, float ay, float az, uint32_t nowMs);
  // Low-pass gravity vector (in screen axes).
  float gx() const { return gx_; }
  float gy() const { return gy_; }
  float gz() const { return gz_; }
  // Tilt relative to the rest position (which adapts after standing still for a few seconds), so that it
  // works whether it lies flat or stands upright. x = left/right, y = forward/back.
  float tiltX() const { return gx_ - rx_; }
  // Upright: forward/back is in z (+ = top towards you -> look down).
  // Lying flat: in y.
  float tiltY() const { return fabsf(ry_) >= fabsf(rz_) ? gz_ - rz_ : gy_ - ry_; }

 private:
  float gx_ = 0, gy_ = 0, gz_ = 1;
  float rx_ = 0, ry_ = 0, rz_ = 1;   // rest position
  bool init_ = false;
  int warmup_ = 0;
  float still_ = 0;          // how long (ms) the device has been lying still
  uint32_t last_ = 0;
  uint32_t shakeWindow_ = 0, lastShake_ = 0, lastPick_ = 0;
  int shakeHits_ = 0;
};

}  // namespace buddy
