#pragma once
// Firmware-updates via GitHub Releases: instelling, status en de voortgangsring tijdens installeren.
// Het netwerkdeel (opvragen, downloaden, flashen) zit in src/net/updater.cpp.

#include <lvgl.h>
#include <stdint.h>

namespace buddy {

enum class UpdateMode : uint8_t { Off, Notify, Auto };
enum class UpdateState : uint8_t { Idle, Checking, UpToDate, Available, Installing, Failed };

const char* updateModeName(UpdateMode m);
bool updateModeFromName(const char* n, UpdateMode& out);
const char* updateStateName(UpdateState s);

// Vergelijkt semver-versies ("v1.2.3" of "1.2.3"; een achtervoegsel als "-rc1" telt niet mee).
// Geeft <0, 0 of >0. Ongeldig = 0.0.0.
int compareVersions(const char* a, const char* b);

// Ring langs de rand van het ronde scherm plus tekst, over het gezicht heen.
class UpdateView {
 public:
  void create(lv_obj_t* parent);
  void update(bool installing, uint8_t pct, const char* version);
  bool visible() const { return visible_; }

 private:
  lv_obj_t* root_ = nullptr;
  lv_obj_t* arc_ = nullptr;
  lv_obj_t* label_ = nullptr;
  bool visible_ = false;
  int lastPct_ = -1;
};

}  // namespace buddy
