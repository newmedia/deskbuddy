#pragma once
// Listening: extract only the sound level from the microphone samples and turn it into events
// (clap, double clap, loud noise, music with a beat). Nothing is recorded or sent.
//
// feed() runs in the audio task (other core), poll()/level() in the main loop: events go
// through a small lock-free ring buffer.

#include <atomic>
#include <stddef.h>
#include <stdint.h>

namespace buddy {

enum class SoundEvent : uint8_t { None, Clap, DoubleClap, Loud, MusicStart, MusicStop, Beat, SpeechStart, SpeechStop };
const char* soundEventName(SoundEvent e);

class MicListener {
 public:
  static constexpr uint32_t kRate = 16000;

  // Audio side: mono 16-bit samples, any block size.
  void feed(const int16_t* s, size_t n);
  // Own sound: while the buddy itself makes sound (and shortly after) don't listen.
  void hold(uint32_t ms) { holdSamples_ = ms * (kRate / 1000); }

  // Main loop side
  SoundEvent poll();
  float level() const { return level_.load(std::memory_order_relaxed); }  // 0..1, for the mouth/dancing
  float db() const { return db_.load(std::memory_order_relaxed); }        // latest level in dBFS
  float floorDb() const { return floor_.load(std::memory_order_relaxed); }
  bool music() const { return music_.load(std::memory_order_relaxed); }
  bool speech() const { return speech_.load(std::memory_order_relaxed); }
  bool enabled() const { return enabled_.load(std::memory_order_relaxed); }
  // Off: also end ongoing states (talking, music). The matching stop events are given by
  // the App itself to the behaviour (the ring buffer has only one writer: the audio task).
  void setEnabled(bool on) {
    enabled_.store(on, std::memory_order_relaxed);
    if (!on) {
      speech_.store(false, std::memory_order_relaxed);
      music_.store(false, std::memory_order_relaxed);
    }
  }
  // Diagnostics: the last 150 frames (1.5 s) as dBFS, oldest first. Returns the number of characters written.
  int trace(char* out, size_t len) const;

 private:
  void frame(float e);  // one frame of 10 ms (energy in dBFS)
  void push(SoundEvent e);

  // Ring buffer (one writer, one reader)
  SoundEvent ring_[16] = {};
  std::atomic<uint8_t> head_{0}, tail_{0};

  std::atomic<float> level_{0}, db_{-90}, floor_{-60};
  std::atomic<bool> music_{false}, speech_{false}, enabled_{true};

  // Only in the audio task
  uint32_t holdSamples_ = 0;
  double acc_ = 0;
  int accN_ = 0;
  uint32_t t_ = 0;  // time in frames (10 ms)
  float floorF_ = -60, lvl_ = 0;
  float hist_[8] = {-90, -90, -90, -90, -90, -90, -90, -90};  // previous frames (for peaks/onsets)
  int histI_ = 0;
  int clapFrames_ = -1;  // >=0: peak in progress, number of frames
  uint32_t lastClap_ = 0;
  bool clapPending_ = false;
  float peakHold_ = -90;  // loudest sound of the last seconds (drops 8 dB/s)  // single clap: only report when no second one follows
  int loudFrames_ = 0;
  uint32_t loudCooldown_ = 0;
  uint32_t onsets_[12] = {};
  int onsetN_ = 0;
  uint32_t lastOnset_ = 0;
  float busy_ = 0;
  float voice_ = 0;  // part of the last seconds with voice-like sound (not too loud)  // part of the last seconds with clearly audible sound (music is almost continuous)
  int8_t trace_[150] = {};
  uint8_t traceI_ = 0;
};

}  // namespace buddy
