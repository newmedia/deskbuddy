#include "sound.h"

#include <math.h>
#include <string.h>

namespace buddy {

namespace {

enum Wave : uint8_t { Sine, Tri, Square, Noise };  // Noise: f0->f1 = kantelfrequentie van het filter

struct Note {
  float f0, f1;      // begin- en eindfrequentie (exponentiële glijding); 0 = stilte
  uint16_t ms;
  uint8_t wave;
  float vol;         // 0..1
  float vibHz = 0, vibDepth = 0;  // vibrato (fractie van frequentie)
  float tremHz = 0;               // tremolo (amplitude)
};

struct Seq {
  const Note* notes;
  uint8_t count;
};

#define SEQ(...)                                         \
  [] {                                                   \
    static const Note n[] = {__VA_ARGS__};               \
    return Seq{n, (uint8_t)(sizeof(n) / sizeof(n[0]))};  \
  }()

const Seq& seqFor(Sound s) {
  static const Seq none{nullptr, 0};
  static const Seq blip = SEQ({900, 1500, 55, Sine, 0.6f});
  static const Seq happy = SEQ({700, 1100, 70, Tri, 0.6f}, {0, 0, 30, Sine, 0}, {900, 1500, 90, Tri, 0.6f});
  static const Seq love = SEQ({523, 523, 110, Sine, 0.5f}, {659, 659, 110, Sine, 0.5f},
                              {784, 1046, 240, Sine, 0.55f, 6, 0.02f});
  static const Seq surprised = SEQ({500, 1900, 160, Sine, 0.6f});
  static const Seq sad = SEQ({720, 360, 420, Sine, 0.5f, 5, 0.03f});
  static const Seq angry = SEQ({200, 160, 120, Square, 0.32f, 18, 0.08f}, {0, 0, 40, Sine, 0},
                               {190, 150, 170, Square, 0.32f, 18, 0.08f});
  static const Seq dizzy = SEQ({600, 600, 750, Tri, 0.5f, 7, 0.35f});
  static const Seq yawn = SEQ({620, 240, 750, Sine, 0.45f, 4, 0.02f});
  static const Seq wake = SEQ({300, 900, 180, Sine, 0.55f}, {900, 1250, 80, Sine, 0.5f});
  static const Seq purr = SEQ({130, 145, 700, Tri, 0.65f, 0, 0, 14});
  static const Seq toggle = SEQ({1200, 1200, 35, Square, 0.22f});
  // Fluitmelodie (vrolijk deuntje, sinus met vibrato)
  static const Seq whistle = SEQ({1568, 1568, 220, Sine, 0.45f, 6, 0.012f}, {1319, 1319, 220, Sine, 0.45f, 6, 0.012f},
                                 {1568, 1760, 180, Sine, 0.45f, 6, 0.012f}, {2093, 2093, 380, Sine, 0.45f, 6, 0.018f},
                                 {0, 0, 120, Sine, 0}, {1760, 1760, 200, Sine, 0.42f, 6, 0.012f},
                                 {1568, 1568, 200, Sine, 0.42f, 6, 0.012f}, {1319, 1175, 450, Sine, 0.4f, 6, 0.02f});
  static const Seq wink = SEQ({2000, 2600, 45, Tri, 0.4f}, {2600, 2600, 140, Sine, 0.35f});
  static const Seq kiss = SEQ({6000, 2500, 35, Noise, 0.5f}, {700, 260, 110, Sine, 0.55f});
  static const Seq rage = SEQ({110, 95, 900, Square, 0.3f, 0, 0, 22}, {7000, 4000, 700, Noise, 0.25f},
                              {120, 90, 800, Square, 0.3f, 0, 0, 26});
  static const Seq fuse = SEQ({2500, 7000, 1400, Noise, 0.22f});
  static const Seq boom = SEQ({4000, 1500, 220, Noise, 0.95f}, {1500, 500, 380, Noise, 0.6f},
                              {500, 120, 600, Noise, 0.32f});
  switch (s) {
    case Sound::Blip: return blip;
    case Sound::Happy: return happy;
    case Sound::Love: return love;
    case Sound::Surprised: return surprised;
    case Sound::Sad: return sad;
    case Sound::Angry: return angry;
    case Sound::Dizzy: return dizzy;
    case Sound::Yawn: return yawn;
    case Sound::Wake: return wake;
    case Sound::Purr: return purr;
    case Sound::Toggle: return toggle;
    case Sound::Whistle: return whistle;
    case Sound::Wink: return wink;
    case Sound::Kiss: return kiss;
    case Sound::Rage: return rage;
    case Sound::Fuse: return fuse;
    case Sound::Boom: return boom;
    default: return none;
  }
}

const char* const kNames[] = {"none", "blip", "happy", "love", "surprised", "sad", "angry", "dizzy",
                              "yawn", "wake", "purr", "toggle", "whistle", "wink", "kiss", "rage",
                              "fuse", "boom"};
constexpr float kTwoPi = 6.2831853f;

}  // namespace

const char* soundName(Sound s) {
  unsigned i = (unsigned)s;
  return i < (unsigned)Sound::Count ? kNames[i] : "?";
}

bool soundFromName(const char* name, Sound& out) {
  for (unsigned i = 1; i < (unsigned)Sound::Count; i++)
    if (!strcmp(name, kNames[i])) {
      out = (Sound)i;
      return true;
    }
  return false;
}

Sound soundForEmotion(Emotion e) {
  switch (e) {
    case Emotion::Happy: return Sound::Happy;
    case Emotion::Love: return Sound::Love;
    case Emotion::Surprised: return Sound::Surprised;
    case Emotion::Sad: return Sound::Sad;
    case Emotion::Angry: return Sound::Angry;
    case Emotion::Dizzy: return Sound::Dizzy;
    case Emotion::Sleepy: return Sound::Yawn;
    default: return Sound::None;
  }
}

void SoundSynth::play(Sound s) {
  if (!enabled_ && s != Sound::Toggle) return;
  pending_ = (uint8_t)s;
}

bool SoundSynth::fill(int16_t* out, size_t n) {
  uint8_t p = pending_.exchange((uint8_t)Sound::None);
  if (p != (uint8_t)Sound::None) {
    playing_ = (Sound)p;
    note_ = 0;
    pos_ = 0;
    phase_ = lfo_ = 0;
  }
  if (playing_ == Sound::None) {
    memset(out, 0, n * sizeof(int16_t));
    return false;
  }

  const float gain = volume_ / 100.0f * 26000.0f;
  for (size_t i = 0; i < n; i++) {
    const Seq& seq = seqFor(playing_);
    if (note_ >= seq.count) {
      playing_ = Sound::None;
      memset(out + i, 0, (n - i) * sizeof(int16_t));
      break;
    }
    const Note& nt = seq.notes[note_];
    uint32_t len = (uint32_t)nt.ms * kRate / 1000;
    float t = (float)pos_ / len;
    float s = 0;
    if (nt.f0 > 0) {
      float f = nt.f0 * powf(nt.f1 / nt.f0, t);
      lfo_ += kTwoPi * (nt.vibHz > 0 ? nt.vibHz : nt.tremHz) / kRate;
      if (nt.vibHz > 0) f *= 1.0f + nt.vibDepth * sinf(lfo_);
      phase_ += f / kRate;
      phase_ -= floorf(phase_);
      switch (nt.wave) {
        case Noise: {
          // 16-bit Galois-LFSR -> witte ruis, door een eenpolig filter met kantelfrequentie f
          noise_ = (noise_ >> 1) ^ (-(int32_t)(noise_ & 1u) & 0xB400u);
          float white = (noise_ & 0xFFFF) / 32768.0f - 1.0f;
          float k = LV_MIN(1.0f, 6.2831853f * f / kRate);
          lp_ += (white - lp_) * k;
          s = lp_ * LV_MIN(3.0f, 1.0f / sqrtf(k + 0.02f));  // lage kantelfreq. compenseren
          break;
        }
        case Tri: s = 4.0f * fabsf(phase_ - 0.5f) - 1.0f; break;
        case Square: s = phase_ < 0.5f ? 0.8f : -0.8f; break;
        default: s = sinf(kTwoPi * phase_); break;
      }
      // Envelope: 4 ms aanzet, 15 ms uitsterven -> geen klikjes.
      float a = LV_MIN(1.0f, pos_ / (0.004f * kRate));
      float r = LV_MIN(1.0f, (len - pos_) / (0.015f * kRate));
      float trem = nt.tremHz > 0 ? 0.55f + 0.45f * sinf(lfo_) : 1.0f;
      s *= nt.vol * a * r * trem;
    }
    out[i] = (int16_t)(s * gain);
    if (++pos_ >= len) {
      pos_ = 0;
      note_++;
    }
  }
  return true;
}

}  // namespace buddy
