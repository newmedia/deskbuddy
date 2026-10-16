#pragma once
// Uiterlijk als bouwdoos: een basis (robot of cartoon), stijlen per onderdeel en kleuren.
// Emoties blijven de oogleden, wenkbrauwhoek, mond enz. sturen; elk onderdeel tekent die op zijn
// eigen manier. Een Look is klein (bewaard in NVS) en wordt ook als JSON over web/BLE gestuurd.

#include <stddef.h>
#include <stdint.h>

namespace buddy {

enum class Skin : uint8_t;  // face.h: Classic (robot) of Mascot (cartoon)

enum class Part : uint8_t { Eyes, Brows, Mouth, Cheeks, Glasses, Hair, Count };

// Stijlen per onderdeel (index 0 = standaard). Niet elke stijl bestaat voor beide basissen; de
// namenlijsten hieronder bepalen wat er per basis kiesbaar is.
enum class EyeStyle : uint8_t { Block, Round, Pill, Ring, Pixel, Visor, Count };
enum class BrowStyle : uint8_t { None, Bar, Thick, Thin, Pixel, Count };
enum class MouthStyle : uint8_t { Auto, Line, Zigzag, Equalizer, Pixel, Count };
enum class CheekStyle : uint8_t { None, Blush, Stripes, Pixel, Freckles, Count };
enum class GlassesStyle : uint8_t { None, Square, Round, Shades, Monocle, Count };
enum class HairStyle : uint8_t { Spiky, Neat, Curly, Mohawk, Bald, Cap, Count };

struct Look {
  uint8_t ver = 1;  // opslagversie (NVS); bij een ander nummer wordt de standaard gebruikt
  uint8_t base = 0; // Skin: 0 = robot, 1 = cartoon
  uint8_t eyes = 0, brows = 0, mouth = 0, cheeks = 1, glasses = 0, hair = 0;
  bool emotionColors = true;  // boos = rood, verliefd = roze, ... (anders altijd de eigen kleur)
  uint32_t primary = 0x6EE7F7;  // robot: ogen + mond   · cartoon: iris
  uint32_t accent = 0x6EE7F7;   // robot: wenkbrauwen   · cartoon: haar + wenkbrauwen
  uint32_t face = 0x000000;     // robot: achtergrond   · cartoon: huidskleur
  uint32_t frame = 0x15171C;    // brilmontuur
  char name[16] = "Robot";
};

// Personages (kant-en-klare looks)
int presetCount();
const Look& preset(int i);
int presetIndex(const char* name);  // -1 = onbekend

// Namen van stijlen: per onderdeel en basis. stylesFor() geeft de toegestane indexen.
const char* partName(Part p);
bool partFromName(const char* n, Part& out);
const char* styleName(Part p, uint8_t style);
bool styleFromName(Part p, const char* n, uint8_t& out);
// Stijlen die voor deze basis bestaan; schrijft indexen in out, geeft het aantal.
int stylesFor(Part p, uint8_t base, uint8_t* out, int max);

Look randomLook(uint32_t seed);
// Look als JSON (voor web/BLE)
void lookJson(const Look& l, char* out, size_t len);
bool parseColor(const char* s, uint32_t& out);  // "#rrggbb" of "rrggbb"

}  // namespace buddy
