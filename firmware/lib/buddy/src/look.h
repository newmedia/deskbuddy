#pragma once
// Appearance as a construction kit: a base (robot or cartoon), styles per part and colours.
// Emotions keep driving the eyelids, eyebrow angle, mouth etc.; each part draws those in its
// own way. A Look is small (stored in NVS) and is also sent as JSON over web/BLE.

#include <stddef.h>
#include <stdint.h>

namespace buddy {

enum class Skin : uint8_t;  // face.h: Classic (robot) or Mascot (cartoon)

enum class Part : uint8_t { Eyes, Brows, Mouth, Cheeks, Glasses, Hair, Lashes, Lips, Earrings, Accessory, Count };

// Styles per part (index 0 = default). Not every style exists for both bases; the
// name lists below determine what can be chosen per base.
enum class EyeStyle : uint8_t { Block, Round, Pill, Ring, Pixel, Visor, Count };
enum class BrowStyle : uint8_t { None, Bar, Thick, Thin, Pixel, Count };
enum class MouthStyle : uint8_t { Auto, Line, Zigzag, Equalizer, Pixel, Count };
enum class CheekStyle : uint8_t { None, Blush, Stripes, Pixel, Freckles, Count };
enum class GlassesStyle : uint8_t { None, Square, Round, Shades, Monocle, Count };
enum class HairStyle : uint8_t { Spiky, Neat, Curly, Mohawk, Bald, Cap, Long, Ponytail, Bun, Bob, Pigtails, LongCurly, Count };
enum class LashStyle : uint8_t { None, Lashes, Liner, Count };
enum class LipStyle : uint8_t { None, Lipstick, Count };
enum class EarringStyle : uint8_t { None, Studs, Hoops, Count };
enum class AccessoryStyle : uint8_t { None, Bow, Headband, Flower, Count };

struct Look {
  uint8_t ver = 2;  // storage version (NVS); with a different number the default is used
  uint8_t base = 0; // Skin: 0 = robot, 1 = cartoon
  uint8_t eyes = 0, brows = 0, mouth = 0, cheeks = 1, glasses = 0, hair = 0;
  uint8_t lashes = 0, lips = 0, earrings = 0, accessory = 0;  // cartoon elements
  bool emotionColors = true;  // angry = red, in love = pink, ... (otherwise always the own colour)
  uint32_t primary = 0x6EE7F7;  // robot: eyes + mouth   · cartoon: iris
  uint32_t accent = 0x6EE7F7;   // robot: eyebrows       · cartoon: hair + eyebrows
  uint32_t face = 0x000000;     // robot: background     · cartoon: skin colour
  uint32_t frame = 0x15171C;    // glasses frame
  uint32_t lip = 0xD6336C;      // lipstick
  uint32_t extra = 0xFF6FA8;    // earrings and hair accessories
  char name[16] = "Robot";
};

// Characters (ready-made looks)
int presetCount();
const Look& preset(int i);
int presetIndex(const char* name);  // -1 = unknown

// Names of styles: per part and base. stylesFor() gives the allowed indices.
const char* partName(Part p);
bool partFromName(const char* n, Part& out);
const char* styleName(Part p, uint8_t style);
bool styleFromName(Part p, const char* n, uint8_t& out);
// Styles that exist for this base; writes indices into out, returns the count.
int stylesFor(Part p, uint8_t base, uint8_t* out, int max);

Look randomLook(uint32_t seed);

// Compact storage for custom profiles: styles at 4 bits each, colours as 3 bytes, name max 12 characters.
constexpr int kPackedLookSize = 38;
constexpr int kMaxProfiles = 8;
void packLook(const Look& l, uint8_t* out);
bool unpackLook(const uint8_t* in, Look& out);
// Look as JSON (for web/BLE)
void lookJson(const Look& l, char* out, size_t len);
bool parseColor(const char* s, uint32_t& out);  // "#rrggbb" or "rrggbb"

}  // namespace buddy
