#include "sound.h"

#include <math.h>
#include <string.h>

namespace buddy {

namespace {

enum Wave : uint8_t { Sine, Tri, Square };

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
    default: return none;
  }
}

const char* const kNames[] = {"none", "blip", "happy", "love", "surprised", "sad",
                              "angry", "dizzy", "yawn", "wake", "purr", "toggle"};
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
