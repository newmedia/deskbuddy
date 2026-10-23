#pragma once
// Screen of the board this build is for, chosen by the PlatformIO env (see docs/xl-port.md):
//   Original (1.75"): round, 466x466   (envs esp32, sim, emu, webemu)
//   XL (2.16"):       square with rounded corners, 480x480   (envs *-xl, -D DESKBUDDY_BOARD_XL)

#include <math.h>

namespace buddy {

#ifdef DESKBUDDY_BOARD_XL
constexpr int kScreen = 480;
constexpr bool kSquare = true;
constexpr const char* kBoardName = "xl";
#else
constexpr int kScreen = 466;
constexpr bool kSquare = false;
constexpr const char* kBoardName = "original";
#endif

constexpr int kScreenHalf = kScreen / 2;
// Corner radius of the visible area: a circle on the Original. For the XL an estimate (Waveshare does
// not document it); check against the real panel.
constexpr float kCornerRadius = kSquare ? 40.0f : kScreen / 2.0f;

// Distance of pixel (x, y) to the edge of the physical screen: <= 0 inside, > 0 outside. For the
// emulators' masks; the firmware itself simply draws the full square.
inline float screenEdgeDistance(int x, int y) {
  const float r = kCornerRadius, lo = r, hi = kScreen - r;
  float px = x + 0.5f, py = y + 0.5f;
  float dx = px < lo ? lo - px : (px > hi ? px - hi : 0);
  float dy = py < lo ? lo - py : (py > hi ? py - hi : 0);
  return sqrtf(dx * dx + dy * dy) - r;
}
inline bool onScreen(int x, int y) { return screenEdgeDistance(x, y) <= 0; }

}  // namespace buddy
