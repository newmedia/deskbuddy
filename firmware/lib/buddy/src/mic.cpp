#include "mic.h"

#include <math.h>
#include <stdio.h>

namespace buddy {

namespace {
constexpr int kFrame = MicListener::kRate / 100;  // 10 ms
const char* const kNames[] = {"none", "clap", "doubleclap", "loud", "music", "musicstop", "beat", "speech", "speechstop"};
}  // namespace

const char* soundEventName(SoundEvent e) {
  unsigned i = (unsigned)e;
  return i < sizeof(kNames) / sizeof(kNames[0]) ? kNames[i] : "?";
}

void MicListener::push(SoundEvent e) {
  uint8_t h = head_.load(std::memory_order_relaxed);
  uint8_t next = (uint8_t)((h + 1) % 16);
  if (next == tail_.load(std::memory_order_acquire)) return;  // full: drop it
  ring_[h] = e;
  head_.store(next, std::memory_order_release);
}

SoundEvent MicListener::poll() {
  uint8_t t = tail_.load(std::memory_order_relaxed);
  if (t == head_.load(std::memory_order_acquire)) return SoundEvent::None;
  SoundEvent e = ring_[t];
  tail_.store((uint8_t)((t + 1) % 16), std::memory_order_release);
  return e;
}

void MicListener::feed(const int16_t* s, size_t n) {
  if (!enabled()) return;
  for (size_t i = 0; i < n; i++) {
    if (holdSamples_) {  // own sound: skip, but time keeps running
      holdSamples_--;
      if (++accN_ >= kFrame) {
        accN_ = 0;
        acc_ = 0;
        t_++;
        clapFrames_ = -1;
        loudFrames_ = 0;
      }
      continue;
    }
    acc_ += (double)s[i] * s[i];
    if (++accN_ >= kFrame) {
      double ms = acc_ / accN_;
      acc_ = 0;
      accN_ = 0;
      frame(ms > 0 ? (float)(10.0 * log10(ms / (32768.0 * 32768.0))) : -90.0f);
    }
  }
}

void MicListener::frame(float e) {
  t_++;
  if (e < -90) e = -90;
  db_.store(e, std::memory_order_relaxed);

  // Average of the previous frames (excluding this frame)
  float prev = 0;
  for (float h : hist_) prev += powf(10, h / 10);
  prev = 10 * log10f(prev / 8 + 1e-12f);

  // Background level = the median of the sound (fixed steps of 0.03 dB per frame up or
  // down, ~3 dB/s). A fan or PC that hums continuously thus counts as background; only
  // what clearly rises above it (voice, clap, music) counts.
  floorF_ += e > floorF_ ? 0.03f : -0.03f;
  if (e < floorF_ - 20) floorF_ -= 0.3f;  // after something loud (reverb gone) return to quiet faster
  if (floorF_ < -80) floorF_ = -80;
  floor_.store(floorF_, std::memory_order_relaxed);
  const float above = e - floorF_;

  // Level for the mouth: fast attack, slow release
  float target = above < 4 ? 0 : (above - 4) / 30;
  if (target > 1) target = 1;
  lvl_ += (target > lvl_ ? 0.5f : 0.06f) * (target - lvl_);
  level_.store(lvl_, std::memory_order_relaxed);

  // Clap: a sudden peak far above the background level that is gone again within ~120 ms.
  if (clapFrames_ < 0) {
    // ...and clearly louder than what sounded in the last seconds (not syllables in a conversation)
    if (above > 24 && e > -48 && e - prev > 16 && (e > peakHold_ + 6 || clapPending_)) clapFrames_ = 0;
  } else {
    clapFrames_++;
    if (above < 12) {
      if (clapFrames_ <= 7) {  // a clap is short: at most ~70 ms (reverb included)
        uint32_t since = t_ - lastClap_;
        if (clapPending_ && since >= 12 && since <= 70) {
          push(SoundEvent::DoubleClap);
          clapPending_ = false;
        } else {
          clapPending_ = true;
          lastClap_ = t_;
        }
      }
      clapFrames_ = -1;
    } else if (clapFrames_ > 7) {
      clapFrames_ = -1;  // too long for a clap: just sound (speech, music)
    }
  }
  if (clapPending_ && t_ - lastClap_ > 70) {  // no second clap came
    push(SoundEvent::Clap);
    clapPending_ = false;
  }

  peakHold_ = e > peakHold_ ? e : peakHold_ - 0.08f;

  // Loud noise: ~1.5 s sustained far above the background level (or just very loud)
  if (loudCooldown_) loudCooldown_--;
  // (without beats: music is loud too, but we dance to that)
  int beats = 0;
  for (int i = 0; i < onsetN_; i++)
    if (t_ - onsets_[i] <= 150) beats++;
  if ((above > 24 || e > -18) && !music() && beats <= 1) loudFrames_++;
  else if (loudFrames_ > 0) loudFrames_ -= 2;
  if (loudFrames_ < 0) loudFrames_ = 0;
  if (loudFrames_ >= 150 && !loudCooldown_) {
    push(SoundEvent::Loud);
    loudCooldown_ = 600;  // 6 s
    loudFrames_ = 0;
  }

  busy_ += ((above > 6 ? 1.0f : 0.0f) - busy_) * 0.004f;  // ~2.5 s

  // Beats: clear energy jump above the recent level, at least 250 ms after the previous one
  bool onset = e - prev > 6 && above > 9 && t_ - lastOnset_ >= 25;
  if (onset) {
    lastOnset_ = t_;
    if (onsetN_ < 12) onsets_[onsetN_++] = t_;
    else {
      for (int i = 0; i < 11; i++) onsets_[i] = onsets_[i + 1];
      onsets_[11] = t_;
    }
  }
  // Music: at least 8 regular beats (330-1000 ms apart, 60-180 bpm) in the last 5 s,
  // and almost continuous sound. Speech has rhythm too, but with pauses and irregular syllables.
  int recent = 0;
  for (int i = 0; i < onsetN_; i++)
    if (t_ - onsets_[i] <= 500) recent++;
  bool isMusic = false;
  if (recent >= 8 && busy_ > 0.7f) {
    float sum = 0, sum2 = 0;
    int k = 0;
    for (int i = onsetN_ - recent + 1; i < onsetN_; i++) {
      float d = (float)(onsets_[i] - onsets_[i - 1]);
      sum += d;
      sum2 += d * d;
      k++;
    }
    float mean = sum / k, sd = sqrtf(fmaxf(0, sum2 / k - mean * mean));
    isMusic = mean >= 33 && mean <= 100 && sd < mean * 0.2f;
  }
  bool was = music();
  if (isMusic != was) {
    // Off only after 2 s without beats (a short pause in a song doesn't count)
    if (isMusic || t_ - lastOnset_ > 200) {
      music_.store(isMusic, std::memory_order_relaxed);
      push(isMusic ? SoundEvent::MusicStart : SoundEvent::MusicStop);
    }
  }
  if (onset && music()) push(SoundEvent::Beat);

  // Talking: a good part of the time voice-like sound (above the background level, not very loud),
  // but with pauses and without a fixed beat. On above 30%, off below 12% (~2 s tail).
  voice_ += ((above > 10 && e < -15 ? 1.0f : 0.0f) - voice_) * 0.005f;
  bool talking = speech();
  if (!talking && voice_ > 0.3f && !music()) {
    speech_.store(true, std::memory_order_relaxed);
    push(SoundEvent::SpeechStart);
  } else if (talking && (voice_ < 0.12f || music())) {
    speech_.store(false, std::memory_order_relaxed);
    push(SoundEvent::SpeechStop);
  }

  hist_[histI_] = e;
  histI_ = (histI_ + 1) % 8;
  trace_[traceI_] = (int8_t)(e < -127 ? -127 : e);
  traceI_ = (uint8_t)((traceI_ + 1) % 150);
}

int MicListener::trace(char* out, size_t len) const {
  size_t n = 0;
  for (int i = 0; i < 150 && n + 6 < len; i++) {
    int v = trace_[(traceI_ + i) % 150];
    n += (size_t)snprintf(out + n, len - n, i ? ",%d" : "%d", v);
  }
  return (int)n;
}

}  // namespace buddy
