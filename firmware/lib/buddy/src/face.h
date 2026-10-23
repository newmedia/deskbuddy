#pragma once
// Procedurally drawn face: two eyes with eyelids, blush, mouth and Z's.
// Depends only on LVGL, so the same code runs on the device and in the simulator.

#include <lvgl.h>

#include "look.h"
#include <math.h>
#include <stdint.h>

namespace buddy {

enum class Emotion : uint8_t {
  Neutral,
  Happy,
  Sad,
  Angry,
  Surprised,
  Sleepy,
  Love,
  Dizzy,
  Asleep,
  Cry,    // crying: tears roll down the cheeks
  Hyper,  // busy (ADHD): wide open eyes, gaze darts everywhere, little hops
  Pain,   // pain: squeezed-shut eyes, clenched teeth, sweat drop, an occasional stab
  Count
};

// Appearance. Classic = cyan robot eyes on black; Mascot = cartoon face with hair, glasses and smile.
enum class Skin : uint8_t { Classic, Mascot, Count };

// Short animations on top of the emotion; afterwards back to the previous emotion.
enum class Action : uint8_t { None, Whistle, Wink, Kiss, Rage, Explode, Sneeze, Laugh, EyeRoll, Hiccup, Count };
// Sound moments within an action (the App attaches a Sound to them).
enum class Cue : uint8_t { None, Whistle, Wink, Kiss, Rage, Fuse, Boom, Inhale, Achoo, Laugh, Sigh, Hiccup };

const char* actionName(Action a);
bool actionFromName(const char* name, Action& out);

const char* skinName(Skin s);
bool skinFromName(const char* name, Skin& out);

const char* emotionName(Emotion e);
bool emotionFromName(const char* name, Emotion& out);

// Critically-ish damped spring: gives movement a bit of "overshoot" and feels livelier than lerp.
struct Spring {
  float x = 0, v = 0, target = 0;
  float k = 180, d = 24;
  void step(float dt) {
    float a = k * (target - x) - d * v;
    v += a * dt;
    x += v * dt;
  }
  void snap(float val) { x = target = val; v = 0; }
  // Settled (and then set exactly on the target, so the image no longer changes)?
  bool settle(float eps) {
    if (fabsf(x - target) < eps && fabsf(v) < eps * 4) {
      x = target;
      v = 0;
      return true;
    }
    return false;
  }
};

class Face {
 public:
  static constexpr int kSize = 466;

  void create(lv_obj_t* parent);
  void update(uint32_t dtMs);

  void setEmotion(Emotion e);
  Emotion emotion() const { return emotion_; }

  // Gaze direction -1..1. hold=true keeps the gaze until releaseLook().
  void lookAt(float x, float y, bool hold = true);
  void releaseLook();
  void blink();
  void bounce(float amount = 1.0f);  // squash & stretch impulse
  void hop(float height = 48);       // little hop: up in an arc, landing with a spring
  // Listening: an ear with sound waves alternately on the left and right, the eyes look along.
  void setListening(bool on);
  bool listening() const { return listening_; }
  void setAutoBlink(bool on) { autoBlink_ = on; }
  void setSkin(Skin s);
  // Matrix face during firmware updates: green eyes with falling code (only the eyes redraw).
  void setMatrix(bool on);
  // Covered by an opaque panel (clock, info, ...): then redraw nothing.
  void setCovered(bool covered);
  // Sound level 0..1 (for the LED equaliser mouth)
  void setSoundLevel(float l) { soundLevel_ = l; }
  bool matrix() const { return matrix_; }
  void play(Action a);
  Action action() const { return action_; }
  bool busy() const { return action_ != Action::None; }
  Cue takeCue() {
    Cue c = cue_;
    cue_ = Cue::None;
    return c;
  }
  Skin skin() const { return skin_; }
  // Full appearance (base, styles per part, colours); see look.h
  void setLook(const Look& l);
  const Look& look() const { return look_; }

  lv_obj_t* obj() const { return obj_; }

 private:
  struct Pose {
    float w, h, r;        // eye size and corner radius
    float lidTop;         // 0..1 upper eyelid closed
    float tilt;           // degrees; + = inner corner down (angry), - = inner corner up (sad)
    float lidBottom;      // 0..1 lower eyelid raised (happy "^ ^" eyes)
    float dy;             // vertical offset of both eyes
    float blush;          // 0..1
    uint32_t color;
    uint8_t mouth;        // MouthType
    bool heart;
  };
  enum MouthType : uint8_t { MouthNone, MouthSmile, MouthFrown, MouthO, MouthFlat };

  struct EyeState {
    Spring w, h, r, lidTop, tilt, lidBottom;
  };

  static Pose poseFor(Emotion e);
  static void drawCb(lv_event_t* e);
  void draw(lv_layer_t* layer);
  void drawEye(lv_layer_t* layer, int side, float cx, float cy, float scale);
  void drawZs(lv_layer_t* layer, lv_color_t color);
  void drawTears(lv_layer_t* layer, float cx, float y0, int side);  // y0 = bottom of the eye
  void drawSweat(lv_layer_t* layer, float x, float y);
  void drawEar(lv_layer_t* layer, bool mascot);  // listening ear (see setListening)
  void drawRobot(lv_layer_t* layer);  // face_robot.cpp
  void drawRobotBrows(lv_layer_t* layer, float cy);
  void drawRobotGlasses(lv_layer_t* layer, float cy);
  void drawPixelShape(lv_layer_t* layer, float x1, float y1, float x2, float y2, float r, bool heart, lv_color_t col);
  void drawMascot(lv_layer_t* layer);
  void drawMascotEye(lv_layer_t* layer, int side, float cx, float cy);
  void drawMascotMouth(lv_layer_t* layer, float cx, float cy);
  void drawHair(lv_layer_t* layer, float dy);
  void mascotTargets();
  // Actions and effects (face_fx.cpp)
  struct Particle {
    float x, y, vx, vy, life, maxLife, size, spin;
    uint8_t type;
    uint32_t color;
  };
  enum ParticleType : uint8_t { PNote, PHeart, PSteam, PSpark, PSymbol, PSmoke, PStar, PDrop };
  void updateAction(float dt);
  void updateParticles(float dt);
  void spawn(uint8_t type, float x, float y, float vx, float vy, float life, float size, uint32_t color);
  void drawEffects(lv_layer_t* layer);
  void drawMatrix(lv_layer_t* layer);
  void drawPucker(lv_layer_t* layer, float cx, float cy, lv_color_t col, bool mascot);
  bool effectsActive() const;
  bool settled();  // nothing moves: then nothing needs to be redrawn (saves a lot of CPU)
  bool stillExceptEar();
  float eyeClosure(int side) const;  // blink + wink/kiss for one eye
  void drawMouth(lv_layer_t* layer, float cx, float cy);
  void applyPose(const Pose& p);
  void invalidate();
  void scheduleNextBlink();
  void scheduleNextSaccade();
  float rnd();

  lv_obj_t* obj_ = nullptr;
  Look look_;
  uint32_t poseColor(Emotion e, uint32_t emotionColor) const;
  bool matrix_ = false;
  int bobPx_ = 0, drawnBobPx_ = 0;  // breathing in whole pixels: only redraw when it really shifts
  bool wasSettled_ = false;
  bool covered_ = false;
  float soundLevel_ = 0;
  float matrixHead_[2][7];  // per eye, per column: position of the head (in characters)
  float matrixSpeed_[2][7];
  // Action state
  Action action_ = Action::None;
  Cue cue_ = Cue::None;
  float actT_ = 0, actNext_ = 0;
  Emotion actPrevEmotion_ = Emotion::Neutral;
  int actStage_ = 0;
  float wink_[2] = {0, 0};   // per-eye extra closure (0 = left, 1 = right)
  bool pucker_ = false;      // puckered mouth (whistling, kiss)
  float swell_ = 1;          // swelling (exploding)
  float shake_ = 0;          // shake amplitude in px
  float tint_ = 0;           // red glow 0..1
  float flash_ = 0;          // white flash 0..1
  float soot_ = 0;           // soot stains 0..1
  static constexpr int kMaxParticles = 40;
  Particle particles_[kMaxParticles];
  int particleCount_ = 0;
  Skin skin_ = Skin::Classic;
  bool fullRedraw_ = true;
  // Mascot-specific
  Spring browAngle_, browRaise_, mouthOpen_, mouthWidth_, hairY_;
  Emotion emotion_ = Emotion::Neutral;
  Pose pose_{};
  EyeState eye_;
  Spring lookX_, lookY_, dy_, blush_, squash_;
  Spring colR_, colG_, colB_;
  float mouthAlpha_ = 0;
  uint8_t mouthShown_ = MouthNone;
  bool heartShown_ = false;
  bool pendingHeart_ = false;

  float t_ = 0;           // seconds since start
  float blinkT_ = -1;     // -1 = no blink in progress, otherwise 0..1
  float nextBlink_ = 2;
  float nextSaccade_ = 1;
  float nextTwinge_ = 0;  // pain: next stab
  int16_t offX_ = 0, offY_ = 0;  // offset of the drawing (bouncing, shaking)
  float hopT_ = -1, hopH_ = 0;
  bool listening_ = false;
  float listenT_ = 0;   // seconds since listening began
  float earPop_ = 0;    // 0..1: how far the ear sticks out
  int earSide_ = 1;
  lv_draw_buf_t* hopSnap_ = nullptr;  // snapshot of the face during a hop (PSRAM)  // hop: progress 0..1 (-1 = none) and height in px
  bool lookHeld_ = false;
  bool autoBlink_ = true;
  uint32_t seed_ = 0x1234567;

  lv_area_t prevBox_{0, 0, kSize - 1, kSize - 1};
  lv_area_t box_{};
};

}  // namespace buddy
