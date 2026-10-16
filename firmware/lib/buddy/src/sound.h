#pragma once
// Kleine synthesizer voor robotgeluidjes: geen samples nodig, alles wordt live berekend.
// play() mag vanaf de hoofdlus, fill() draait in de audio-task/callback (andere thread).

#include <atomic>
#include <stddef.h>
#include <stdint.h>

#include "face.h"

namespace buddy {

enum class Sound : uint8_t {
  None,
  Blip,
  Happy,
  Love,
  Surprised,
  Sad,
  Angry,
  Dizzy,
  Yawn,
  Wake,
  Purr,
  Toggle,
  Whistle,
  Wink,
  Kiss,
  Rage,
  Fuse,
  Boom,
  Inhale,
  Achoo,
  Laugh,
  Sigh,
  Hiccup,
  Count
};

const char* soundName(Sound s);
bool soundFromName(const char* name, Sound& out);
Sound soundForEmotion(Emotion e);  // Sound::None als er geen geluid bij hoort

class SoundSynth {
 public:
  static constexpr uint32_t kRate = 16000;

  void play(Sound s);
  // Vult n mono-samples (int16). Geeft true als er geluid speelde (handig voor de versterker).
  bool fill(int16_t* out, size_t n);

  void setEnabled(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }
  void setVolume(uint8_t pct) { volume_ = pct > 100 ? 100 : pct; }
  uint8_t volume() const { return volume_; }
  // Geluidsniveau van het laatst gevulde blok, 0..1 (voor de equalizer-mond)
  float level() const { return level_; }
  bool active() const { return playing_ != Sound::None || pending_ != (uint8_t)Sound::None; }

 private:
  std::atomic<uint8_t> pending_{0};
  std::atomic<bool> enabled_{true};
  std::atomic<uint8_t> volume_{70};
  std::atomic<float> level_{0};
  // Alleen in fill() gebruikt:
  Sound playing_ = Sound::None;
  uint8_t note_ = 0;
  uint32_t pos_ = 0;   // sample binnen huidige noot
  float phase_ = 0, lfo_ = 0;
  uint32_t noise_ = 0xACE1u;  // LFSR voor ruis
  float lp_ = 0;              // laagdoorlaat voor gekleurde ruis
};

}  // namespace buddy
