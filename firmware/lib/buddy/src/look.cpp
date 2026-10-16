#include "look.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace buddy {

namespace {

const char* const kParts[] = {"eyes", "brows", "mouth", "cheeks", "glasses", "hair"};
const char* const kEyes[] = {"block", "round", "pill", "ring", "pixel", "visor"};
const char* const kBrows[] = {"none", "bar", "thick", "thin", "pixel"};
const char* const kMouths[] = {"auto", "line", "zigzag", "equalizer", "pixel"};
const char* const kCheeks[] = {"none", "blush", "stripes", "pixel", "freckles"};
const char* const kGlasses[] = {"none", "square", "round", "shades", "monocle"};
const char* const kHair[] = {"spiky", "neat", "curly", "mohawk", "bald", "cap"};

struct Names {
  const char* const* names;
  uint8_t count;
};
Names names(Part p) {
  switch (p) {
    case Part::Eyes: return {kEyes, 6};
    case Part::Brows: return {kBrows, 5};
    case Part::Mouth: return {kMouths, 5};
    case Part::Cheeks: return {kCheeks, 5};
    case Part::Glasses: return {kGlasses, 5};
    case Part::Hair: return {kHair, 6};
    default: return {nullptr, 0};
  }
}

Look make(const char* name, uint8_t base, uint8_t eyes, uint8_t brows, uint8_t mouth, uint8_t cheeks, uint8_t glasses,
          uint8_t hair, uint32_t primary, uint32_t accent, uint32_t face, uint32_t frame, bool emo = true) {
  Look l;
  l.base = base;
  l.eyes = eyes;
  l.brows = brows;
  l.mouth = mouth;
  l.cheeks = cheeks;
  l.glasses = glasses;
  l.hair = hair;
  l.primary = primary;
  l.accent = accent;
  l.face = face;
  l.frame = frame;
  l.emotionColors = emo;
  snprintf(l.name, sizeof(l.name), "%s", name);
  return l;
}

using E = EyeStyle;
using B = BrowStyle;
using M = MouthStyle;
using C = CheekStyle;
using G = GlassesStyle;
using H = HairStyle;
#define I(x) (uint8_t)(x)

const Look kPresets[] = {
    //    naam          basis ogen          wenkbr.      mond             wangen          bril            haar
    make("Robot", 0, I(E::Block), I(B::None), I(M::Auto), I(C::Blush), I(G::None), 0, 0x6EE7F7, 0x6EE7F7, 0x000000, 0x6EE7F7),
    make("Mascotte", 1, I(E::Block), I(B::Thick), I(M::Auto), I(C::Blush), I(G::Square), I(H::Spiky), 0x5A7CEB, 0x0B2290,
         0xF5C99B, 0x15171C),
    make("Retro", 0, I(E::Ring), I(B::Thin), I(M::Equalizer), I(C::None), I(G::None), 0, 0xFFB347, 0xFFB347, 0x000000,
         0xFFB347, false),
    make("Pixel", 0, I(E::Pixel), I(B::Pixel), I(M::Pixel), I(C::Pixel), I(G::None), 0, 0x5BE37D, 0x5BE37D, 0x000000,
         0x5BE37D),
    make("Cool", 0, I(E::Pill), I(B::Bar), I(M::Line), I(C::None), I(G::Shades), 0, 0xE6F0F2, 0xE6F0F2, 0x000000,
         0x9AA3AB),
    make("Synth", 0, I(E::Visor), I(B::None), I(M::Zigzag), I(C::Stripes), I(G::None), 0, 0xFF4FA3, 0xB89CFF, 0x12001F,
         0xB89CFF, false),
    make("Professor", 1, I(E::Block), I(B::Thin), I(M::Auto), I(C::Blush), I(G::Round), I(H::Bald), 0x6B8E4E, 0xB0B4BA,
         0xE8B98A, 0x8A6A2E),
    make("Rocker", 1, I(E::Block), I(B::Thick), I(M::Auto), I(C::None), I(G::Shades), I(H::Mohawk), 0x3A7BD5, 0xFF3FA4,
         0xF2C4A0, 0x111111),
    make("Krullebol", 1, I(E::Round), I(B::Thick), I(M::Auto), I(C::Freckles), I(G::None), I(H::Curly), 0x3B2A1A,
         0x4A2C17, 0x8D5524, 0x15171C),
    make("Sportief", 1, I(E::Block), I(B::Thin), I(M::Line), I(C::Blush), I(G::None), I(H::Cap), 0x2F7D4F, 0xD62828,
         0xFFDBB5, 0x15171C),
};
constexpr int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

// Wat er per basis kiesbaar is (robot = 0, cartoon = 1)
bool allowed(Part p, uint8_t base, uint8_t s) {
  if (base == 0) {
    if (p == Part::Hair) return false;
    if (p == Part::Cheeks) return s != (uint8_t)CheekStyle::Freckles;
    return true;
  }
  switch (p) {
    case Part::Eyes: return s == (uint8_t)EyeStyle::Block || s == (uint8_t)EyeStyle::Round;  // normaal, stipjes
    case Part::Brows: return s != (uint8_t)BrowStyle::Pixel;
    case Part::Mouth: return s == (uint8_t)MouthStyle::Auto || s == (uint8_t)MouthStyle::Line;
    case Part::Cheeks: return s == (uint8_t)CheekStyle::None || s == (uint8_t)CheekStyle::Blush ||
                              s == (uint8_t)CheekStyle::Freckles;
    default: return true;
  }
}

}  // namespace

int presetCount() { return kPresetCount; }
const Look& preset(int i) { return kPresets[(i % kPresetCount + kPresetCount) % kPresetCount]; }

int presetIndex(const char* name) {
  for (int i = 0; i < kPresetCount; i++)
    if (!strcasecmp(name, kPresets[i].name)) return i;
  char* end;
  long n = strtol(name, &end, 10);
  return (*end == 0 && n >= 1 && n <= kPresetCount) ? (int)n - 1 : -1;
}

const char* partName(Part p) { return (unsigned)p < (unsigned)Part::Count ? kParts[(int)p] : "?"; }

bool partFromName(const char* n, Part& out) {
  for (int i = 0; i < (int)Part::Count; i++)
    if (!strcmp(n, kParts[i])) {
      out = (Part)i;
      return true;
    }
  return false;
}

const char* styleName(Part p, uint8_t s) {
  Names n = names(p);
  return s < n.count ? n.names[s] : "?";
}

bool styleFromName(Part p, const char* name, uint8_t& out) {
  Names n = names(p);
  for (uint8_t i = 0; i < n.count; i++)
    if (!strcmp(name, n.names[i])) {
      out = i;
      return true;
    }
  return false;
}

int stylesFor(Part p, uint8_t base, uint8_t* out, int max) {
  Names n = names(p);
  int k = 0;
  for (uint8_t i = 0; i < n.count && k < max; i++)
    if (allowed(p, base, i)) out[k++] = i;
  return k;
}

Look randomLook(uint32_t seed) {
  auto rnd = [&seed](int n) {
    seed = seed * 1664525u + 1013904223u;
    return (int)((seed >> 8) % (uint32_t)n);
  };
  // Kleuren uit een paar mooie paletten (willekeurige RGB wordt al snel lelijk)
  static const uint32_t robotColors[] = {0x6EE7F7, 0xFFB347, 0x5BE37D, 0xFF4FA3, 0xB89CFF, 0xE6F0F2, 0xFF5A4E, 0x8AB4FF};
  static const uint32_t skins[] = {0xF5C99B, 0xE8B98A, 0xD9A066, 0xB57A4A, 0x8D5524, 0x5C3A21, 0xFFDBB5};
  static const uint32_t hairs[] = {0x0B2290, 0x2B1B10, 0x4A2C17, 0xC9A227, 0xB0B4BA, 0xD62828, 0xFF3FA4, 0x111111};
  static const uint32_t irises[] = {0x5A7CEB, 0x3B2A1A, 0x2F7D4F, 0x6B8E4E, 0x7A4E2D};

  Look l = preset(rnd(2));  // begin bij Robot of Mascotte
  uint8_t pick[8];
  for (int p = 0; p < (int)Part::Count; p++) {
    int n = stylesFor((Part)p, l.base, pick, 8);
    if (!n) continue;
    uint8_t s = pick[rnd(n)];
    switch ((Part)p) {
      case Part::Eyes: l.eyes = s; break;
      case Part::Brows: l.brows = s; break;
      case Part::Mouth: l.mouth = s; break;
      case Part::Cheeks: l.cheeks = s; break;
      case Part::Glasses: l.glasses = rnd(3) ? (uint8_t)GlassesStyle::None : s; break;  // niet te vaak een bril
      case Part::Hair: l.hair = s; break;
      default: break;
    }
  }
  if (l.base == 0) {
    l.primary = robotColors[rnd(8)];
    l.accent = rnd(2) ? l.primary : robotColors[rnd(8)];
    l.frame = l.accent;
  } else {
    l.face = skins[rnd(7)];
    l.accent = hairs[rnd(8)];
    l.primary = irises[rnd(5)];
  }
  snprintf(l.name, sizeof(l.name), "Verrassing");
  return l;
}

void lookJson(const Look& l, char* out, size_t len) {
  snprintf(out, len,
           "{\"name\":\"%s\",\"base\":\"%s\",\"eyes\":\"%s\",\"brows\":\"%s\",\"mouth\":\"%s\",\"cheeks\":\"%s\","
           "\"glasses\":\"%s\",\"hair\":\"%s\",\"primary\":\"#%06lx\",\"accent\":\"#%06lx\",\"face\":\"#%06lx\","
           "\"frame\":\"#%06lx\",\"emotionColors\":%s}",
           l.name, l.base ? "cartoon" : "robot", styleName(Part::Eyes, l.eyes), styleName(Part::Brows, l.brows),
           styleName(Part::Mouth, l.mouth), styleName(Part::Cheeks, l.cheeks), styleName(Part::Glasses, l.glasses),
           styleName(Part::Hair, l.hair), (unsigned long)l.primary, (unsigned long)l.accent, (unsigned long)l.face,
           (unsigned long)l.frame, l.emotionColors ? "true" : "false");
}

bool parseColor(const char* s, uint32_t& out) {
  if (*s == '#') s++;
  if (strlen(s) != 6) return false;
  char* end;
  unsigned long v = strtoul(s, &end, 16);
  if (*end) return false;
  out = (uint32_t)v;
  return true;
}

}  // namespace buddy
