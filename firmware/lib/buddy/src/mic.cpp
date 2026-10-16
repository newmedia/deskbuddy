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
  if (next == tail_.load(std::memory_order_acquire)) return;  // vol: laat vallen
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
    if (holdSamples_) {  // eigen geluid: overslaan, maar de tijd loopt door
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

  // Gemiddelde van de vorige frames (zonder dit frame)
  float prev = 0;
  for (float h : hist_) prev += powf(10, h / 10);
  prev = 10 * log10f(prev / 8 + 1e-12f);

  // Achtergrondniveau = de mediaan van het geluid (vaste stapjes van 0,03 dB per frame omhoog of
  // omlaag, ~3 dB/s). Een ventilator of pc die continu wat ruist, telt zo als achtergrond; alleen
  // wat daar duidelijk bovenuit komt (stem, klap, muziek) telt.
  floorF_ += e > floorF_ ? 0.03f : -0.03f;
  if (e < floorF_ - 20) floorF_ -= 0.3f;  // na iets hards (galm weg) sneller terug naar de rust
  if (floorF_ < -80) floorF_ = -80;
  floor_.store(floorF_, std::memory_order_relaxed);
  const float above = e - floorF_;

  // Niveau voor de mond: snel aan, langzaam uit
  float target = above < 4 ? 0 : (above - 4) / 30;
  if (target > 1) target = 1;
  lvl_ += (target > lvl_ ? 0.5f : 0.06f) * (target - lvl_);
  level_.store(lvl_, std::memory_order_relaxed);

  // Klap: een plotse piek ver boven het achtergrondniveau die binnen ~120 ms weer weg is.
  if (clapFrames_ < 0) {
    // ...en duidelijk harder dan wat er de laatste seconden klonk (lettergrepen in een gesprek niet)
    if (above > 24 && e > -48 && e - prev > 16 && (e > peakHold_ + 6 || clapPending_)) clapFrames_ = 0;
  } else {
    clapFrames_++;
    if (above < 12) {
      if (clapFrames_ <= 7) {  // een klap is kort: hooguit ~70 ms (galm meegerekend)
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
      clapFrames_ = -1;  // te lang voor een klap: gewoon geluid (spraak, muziek)
    }
  }
  if (clapPending_ && t_ - lastClap_ > 70) {  // geen tweede klap gekomen
    push(SoundEvent::Clap);
    clapPending_ = false;
  }

  peakHold_ = e > peakHold_ ? e : peakHold_ - 0.08f;

  // Hard lawaai: ~1,5 s aanhoudend ver boven het achtergrondniveau (of gewoon heel hard)
  if (loudCooldown_) loudCooldown_--;
  // (zonder beats: muziek is ook hard, maar daar dansen we op)
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

  busy_ += ((above > 6 ? 1.0f : 0.0f) - busy_) * 0.004f;  // ~2,5 s

  // Beats: duidelijke energiesprong boven het recente niveau, minstens 250 ms na de vorige
  bool onset = e - prev > 6 && above > 9 && t_ - lastOnset_ >= 25;
  if (onset) {
    lastOnset_ = t_;
    if (onsetN_ < 12) onsets_[onsetN_++] = t_;
    else {
      for (int i = 0; i < 11; i++) onsets_[i] = onsets_[i + 1];
      onsets_[11] = t_;
    }
  }
  // Muziek: minstens 8 regelmatige beats (330-1000 ms uit elkaar, 60-180 bpm) in de laatste 5 s,
  // en bijna continu geluid. Spraak heeft ook ritme, maar met pauzes en onregelmatige lettergrepen.
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
    // Uit pas na 2 s zonder beats (een korte pauze in een nummer telt niet)
    if (isMusic || t_ - lastOnset_ > 200) {
      music_.store(isMusic, std::memory_order_relaxed);
      push(isMusic ? SoundEvent::MusicStart : SoundEvent::MusicStop);
    }
  }
  if (onset && music()) push(SoundEvent::Beat);

  // Praten: een flink deel van de tijd stem-achtig geluid (boven het achtergrondniveau, niet keihard),
  // maar met pauzes en zonder vaste beat. Aan boven 30%, uit onder 12% (~2 s naloop).
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
