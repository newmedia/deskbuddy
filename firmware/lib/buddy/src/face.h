#pragma once
// Procedureel getekend gezicht: twee ogen met oogleden, blosjes, mond en Z'jes.
// Alleen afhankelijk van LVGL, zodat dezelfde code op het apparaat en in de simulator draait.

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
  Cry,    // huilen: tranen rollen over de wangen
  Hyper,  // druk (ADHD): wijd open ogen, blik schiet alle kanten op, kleine sprongetjes
  Pain,   // pijn: dichtgeknepen ogen, tanden op elkaar, zweetdruppel, af en toe een steek
  Count
};

// Uiterlijk. Classic = cyaan robotogen op zwart; Mascot = cartoongezicht met haar, bril en lach.
enum class Skin : uint8_t { Classic, Mascot, Count };

// Korte animaties bovenop de emotie; na afloop terug naar de vorige emotie.
enum class Action : uint8_t { None, Whistle, Wink, Kiss, Rage, Explode, Sneeze, Laugh, EyeRoll, Hiccup, Count };
// Geluidsmomenten binnen een actie (de App koppelt er een Sound aan).
enum class Cue : uint8_t { None, Whistle, Wink, Kiss, Rage, Fuse, Boom, Inhale, Achoo, Laugh, Sigh, Hiccup };

const char* actionName(Action a);
bool actionFromName(const char* name, Action& out);

const char* skinName(Skin s);
bool skinFromName(const char* name, Skin& out);

const char* emotionName(Emotion e);
bool emotionFromName(const char* name, Emotion& out);

// Kritisch-ish gedempte veer: geeft beweging een beetje "overshoot" en voelt levendiger dan lerp.
struct Spring {
  float x = 0, v = 0, target = 0;
  float k = 180, d = 24;
  void step(float dt) {
    float a = k * (target - x) - d * v;
    v += a * dt;
    x += v * dt;
  }
  void snap(float val) { x = target = val; v = 0; }
  // Stilgevallen (en dan precies op het doel gezet, zodat het beeld niet meer verandert)?
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

  // Kijkrichting -1..1. hold=true houdt de blik vast tot releaseLook().
  void lookAt(float x, float y, bool hold = true);
  void releaseLook();
  void blink();
  void bounce(float amount = 1.0f);  // squash & stretch-impuls
  void hop(float height = 48);       // sprongetje: omhoog in een boog, landen met een veer
  void setAutoBlink(bool on) { autoBlink_ = on; }
  void setSkin(Skin s);
  // Matrix-gezicht tijdens firmware-updates: groene ogen met vallende code (alleen de ogen hertekenen).
  void setMatrix(bool on);
  // Afgedekt door een ondoorzichtig paneel (klok, info, ...): dan niets hertekenen.
  void setCovered(bool covered);
  // Geluidsniveau 0..1 (voor de LED-equalizer-mond)
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
  // Volledig uiterlijk (basis, stijlen per onderdeel, kleuren); zie look.h
  void setLook(const Look& l);
  const Look& look() const { return look_; }

  lv_obj_t* obj() const { return obj_; }

 private:
  struct Pose {
    float w, h, r;        // oogmaat en hoekradius
    float lidTop;         // 0..1 bovenooglid dicht
    float tilt;           // graden; + = binnenhoek omlaag (boos), - = binnenhoek omhoog (verdrietig)
    float lidBottom;      // 0..1 onderooglid omhoog (blije "^ ^"-ogen)
    float dy;             // verticale verschuiving van beide ogen
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
  void drawTears(lv_layer_t* layer, float cx, float y0, int side);  // y0 = onderkant van het oog
  void drawSweat(lv_layer_t* layer, float x, float y);
  void drawRobot(lv_layer_t* layer);  // face_robot.cpp
  void drawRobotBrows(lv_layer_t* layer, float cy);
  void drawRobotGlasses(lv_layer_t* layer, float cy);
  void drawPixelShape(lv_layer_t* layer, float x1, float y1, float x2, float y2, float r, bool heart, lv_color_t col);
  void drawMascot(lv_layer_t* layer);
  void drawMascotEye(lv_layer_t* layer, int side, float cx, float cy);
  void drawMascotMouth(lv_layer_t* layer, float cx, float cy);
  void drawHair(lv_layer_t* layer, float dy);
  void mascotTargets();
  // Acties en effecten (face_fx.cpp)
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
  bool settled();  // niets beweegt: dan hoeft er niets hertekend te worden (scheelt veel CPU)
  float eyeClosure(int side) const;  // knipper + knipoog/kus voor één oog
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
  int bobPx_ = 0, drawnBobPx_ = 0;  // ademhaling in hele pixels: alleen hertekenen als hij echt verschuift
  bool wasSettled_ = false;
  bool covered_ = false;
  float soundLevel_ = 0;
  float matrixHead_[2][7];  // per oog, per kolom: positie van de kop (in tekens)
  float matrixSpeed_[2][7];
  // Actie-status
  Action action_ = Action::None;
  Cue cue_ = Cue::None;
  float actT_ = 0, actNext_ = 0;
  Emotion actPrevEmotion_ = Emotion::Neutral;
  int actStage_ = 0;
  float wink_[2] = {0, 0};   // per oog extra sluiting (0 = links, 1 = rechts)
  bool pucker_ = false;      // getuite mond (fluiten, kusje)
  float swell_ = 1;          // opzwellen (ontploffen)
  float shake_ = 0;          // trilamplitude in px
  float tint_ = 0;           // rode gloed 0..1
  float flash_ = 0;          // witte flits 0..1
  float soot_ = 0;           // roetvlekken 0..1
  static constexpr int kMaxParticles = 40;
  Particle particles_[kMaxParticles];
  int particleCount_ = 0;
  Skin skin_ = Skin::Classic;
  bool fullRedraw_ = true;
  // Mascotte-specifiek
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

  float t_ = 0;           // seconden sinds start
  float blinkT_ = -1;     // -1 = geen knipper bezig, anders 0..1
  float nextBlink_ = 2;
  float nextSaccade_ = 1;
  float nextTwinge_ = 0;  // pijn: volgende steek
  int16_t offX_ = 0, offY_ = 0;  // verschuiving van de tekening (stuiteren, trillen)
  float hopT_ = -1, hopH_ = 0;
  lv_draw_buf_t* hopSnap_ = nullptr;  // momentopname van het gezicht tijdens een sprongetje (PSRAM)  // sprongetje: voortgang 0..1 (-1 = geen) en hoogte in px
  bool lookHeld_ = false;
  bool autoBlink_ = true;
  uint32_t seed_ = 0x1234567;

  lv_area_t prevBox_{0, 0, kSize - 1, kSize - 1};
  lv_area_t box_{};
};

}  // namespace buddy
