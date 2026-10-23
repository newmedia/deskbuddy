#include "gestures.h"

#include <math.h>
#include <stdlib.h>

#include "screen.h"

namespace buddy {

namespace {
constexpr int kTapMove = 22;          // px
constexpr uint32_t kTapMs = 280;
constexpr uint32_t kDoubleMs = 320;
constexpr uint32_t kLongMs = 650;
constexpr int kSwipeMin = 90;         // px
constexpr uint32_t kSwipeMs = 600;
constexpr float kPetPath = 260;       // px back and forth
constexpr float kHalf = kScreenHalf;

GestureEvent ev(Gesture g, int x, int y) {
  GestureEvent e;
  e.type = g;
  e.x = (x - kHalf) / kHalf;
  e.y = (y - kHalf) / kHalf;
  return e;
}
}  // namespace

const char* gestureName(Gesture g) {
  switch (g) {
    case Gesture::Tap: return "tap";
    case Gesture::DoubleTap: return "double_tap";
    case Gesture::LongPress: return "long_press";
    case Gesture::Pet: return "pet";
    case Gesture::SwipeLeft: return "swipe_left";
    case Gesture::SwipeRight: return "swipe_right";
    case Gesture::SwipeUp: return "swipe_up";
    case Gesture::SwipeDown: return "swipe_down";
    case Gesture::Shake: return "shake";
    case Gesture::PickUp: return "pick_up";
    default: return "none";
  }
}

GestureEvent TouchGestures::feed(bool pressed, int x, int y, uint32_t now) {
  if (pressed && !down_) {
    down_ = true;
    longFired_ = moved_ = false;
    sx_ = lx_ = x;
    sy_ = ly_ = y;
    t0_ = now;
    path_ = 0;
    lastDirX_ = reversals_ = 0;
    return {};
  }

  if (pressed && down_) {
    int dx = x - lx_, dy = y - ly_;
    path_ += sqrtf((float)(dx * dx + dy * dy));
    if (abs(x - sx_) > kTapMove || abs(y - sy_) > kTapMove) moved_ = true;
    if (abs(dx) > 4) {
      int dir = dx > 0 ? 1 : -1;
      if (lastDirX_ != 0 && dir != lastDirX_) reversals_++;
      lastDirX_ = dir;
    }
    lx_ = x;
    ly_ = y;
    if (reversals_ >= 2 && path_ > kPetPath) {
      reversals_ = 0;
      path_ = 0;
      return ev(Gesture::Pet, x, y);
    }
    if (!moved_ && !longFired_ && now - t0_ >= kLongMs) {
      longFired_ = true;
      return ev(Gesture::LongPress, x, y);
    }
    return {};
  }

  if (!pressed && down_) {
    down_ = false;
    uint32_t dur = now - t0_;
    int dx = lx_ - sx_, dy = ly_ - sy_;
    if (longFired_) return {};
    if (!moved_ && dur <= kTapMs) {
      if (lastTap_ && now - lastTap_ <= kDoubleMs) {
        lastTap_ = 0;
        return ev(Gesture::DoubleTap, lx_, ly_);
      }
      lastTap_ = now;
      return ev(Gesture::Tap, lx_, ly_);
    }
    if (dur <= kSwipeMs && reversals_ < 2) {
      if (abs(dx) >= kSwipeMin && abs(dx) > abs(dy))
        return ev(dx > 0 ? Gesture::SwipeRight : Gesture::SwipeLeft, lx_, ly_);
      if (abs(dy) >= kSwipeMin && abs(dy) > abs(dx))
        return ev(dy > 0 ? Gesture::SwipeDown : Gesture::SwipeUp, lx_, ly_);
    }
  }
  return {};
}

GestureEvent MotionGestures::feed(float ax, float ay, float az, uint32_t now) {
  uint32_t dt = last_ ? now - last_ : 20;
  last_ = now;
  float mag0 = ax * ax + ay * ay + az * az;
  if (!init_ && mag0 < 0.25f) return {};  // first measurements can be zero
  if (!init_) {
    init_ = true;
    gx_ = rx_ = ax;
    gy_ = ry_ = ay;
    gz_ = rz_ = az;
  }
  const float a = 0.08f;
  gx_ += (ax - gx_) * a;
  gy_ += (ay - gy_) * a;
  gz_ += (az - gz_) * a;
  // Rest position: follow directly for the first 1.5 s (start-up), then slowly (~5 s when
  // it stands still), so that a skewed position counts as "straight" after a while.
  if (warmup_ < 75) warmup_++;
  const float r = warmup_ < 75 ? 1.0f : (still_ > 1500 ? 0.004f : 0.0015f);
  rx_ += (gx_ - rx_) * r;
  ry_ += (gy_ - ry_) * r;
  rz_ += (gz_ - rz_) * r;

  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float dev = fabsf(mag - 1.0f);

  // Shaking: several hard peaks within a short window.
  if (dev > 0.9f) {
    if (now - shakeWindow_ > 700) {
      shakeWindow_ = now;
      shakeHits_ = 0;
    }
    if (++shakeHits_ >= 4 && now - lastShake_ > 2500) {
      lastShake_ = now;
      shakeHits_ = 0;
      still_ = 0;
      GestureEvent e;
      e.type = Gesture::Shake;
      return e;
    }
  }

  // Picking up: after lying still for a while, a clear (but not shake) movement.
  if (dev < 0.06f) {
    still_ += dt;
  } else if (dev > 0.22f && dev < 0.9f && still_ > 1500 && now - lastPick_ > 4000) {
    still_ = 0;
    lastPick_ = now;
    GestureEvent e;
    e.type = Gesture::PickUp;
    return e;
  } else if (dev > 0.1f) {
    still_ = 0;
  }
  return {};
}

}  // namespace buddy
