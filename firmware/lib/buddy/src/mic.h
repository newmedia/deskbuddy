#pragma once
// Luisteren: uit de microfoon-samples alleen het geluidsniveau halen en daar gebeurtenissen van maken
// (klap, dubbele klap, hard lawaai, muziek met beat). Er wordt niets opgenomen of verstuurd.
//
// feed() draait in de audio-task (ander core), poll()/level() in de hoofdlus: de gebeurtenissen gaan
// via een kleine lock-vrije ringbuffer.

#include <atomic>
#include <stddef.h>
#include <stdint.h>

namespace buddy {

enum class SoundEvent : uint8_t { None, Clap, DoubleClap, Loud, MusicStart, MusicStop, Beat, SpeechStart, SpeechStop };
const char* soundEventName(SoundEvent e);

class MicListener {
 public:
  static constexpr uint32_t kRate = 16000;

  // Audio-kant: mono 16-bit samples, willekeurige blokgrootte.
  void feed(const int16_t* s, size_t n);
  // Eigen geluid: zolang de buddy zelf geluid maakt (en kort daarna) niet luisteren.
  void hold(uint32_t ms) { holdSamples_ = ms * (kRate / 1000); }

  // Hoofdlus-kant
  SoundEvent poll();
  float level() const { return level_.load(std::memory_order_relaxed); }  // 0..1, voor de mond/het dansen
  float db() const { return db_.load(std::memory_order_relaxed); }        // laatste niveau in dBFS
  float floorDb() const { return floor_.load(std::memory_order_relaxed); }
  bool music() const { return music_.load(std::memory_order_relaxed); }
  bool speech() const { return speech_.load(std::memory_order_relaxed); }
  bool enabled() const { return enabled_.load(std::memory_order_relaxed); }
  // Uit: ook lopende toestanden (praten, muziek) beëindigen. De bijbehorende stop-gebeurtenissen geeft
  // de App zelf aan het gedrag (de ringbuffer heeft maar één schrijver: de audio-task).
  void setEnabled(bool on) {
    enabled_.store(on, std::memory_order_relaxed);
    if (!on) {
      speech_.store(false, std::memory_order_relaxed);
      music_.store(false, std::memory_order_relaxed);
    }
  }
  // Diagnose: de laatste 150 frames (1,5 s) als dBFS, oudste eerst. Geeft het aantal geschreven tekens.
  int trace(char* out, size_t len) const;

 private:
  void frame(float e);  // één frame van 10 ms (energie in dBFS)
  void push(SoundEvent e);

  // Ringbuffer (één schrijver, één lezer)
  SoundEvent ring_[16] = {};
  std::atomic<uint8_t> head_{0}, tail_{0};

  std::atomic<float> level_{0}, db_{-90}, floor_{-60};
  std::atomic<bool> music_{false}, speech_{false}, enabled_{true};

  // Alleen in de audio-task
  uint32_t holdSamples_ = 0;
  double acc_ = 0;
  int accN_ = 0;
  uint32_t t_ = 0;  // tijd in frames (10 ms)
  float floorF_ = -60, lvl_ = 0;
  float hist_[8] = {-90, -90, -90, -90, -90, -90, -90, -90};  // vorige frames (voor pieken/onsets)
  int histI_ = 0;
  int clapFrames_ = -1;  // >=0: piek bezig, aantal frames
  uint32_t lastClap_ = 0;
  bool clapPending_ = false;
  float peakHold_ = -90;  // luidste geluid van de laatste seconden (zakt 8 dB/s)  // enkele klap: pas melden als er geen tweede komt
  int loudFrames_ = 0;
  uint32_t loudCooldown_ = 0;
  uint32_t onsets_[12] = {};
  int onsetN_ = 0;
  uint32_t lastOnset_ = 0;
  float busy_ = 0;
  float voice_ = 0;  // deel van de laatste seconden met stem-achtig geluid (niet te hard)  // deel van de laatste seconden met duidelijk geluid (muziek is bijna continu)
  int8_t trace_[150] = {};
  uint8_t traceI_ = 0;
};

}  // namespace buddy
