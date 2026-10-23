#pragma once
// Firmware updates via GitHub Releases: setting, status and the progress ring during installation.
// The network part (query, download, flash) lives in src/net/updater.cpp.

#include <lvgl.h>
#include <stdint.h>

namespace buddy {

enum class UpdateMode : uint8_t { Off, Notify, Auto };
enum class UpdateState : uint8_t { Idle, Checking, UpToDate, Available, Installing, Failed };

const char* updateModeName(UpdateMode m);
bool updateModeFromName(const char* n, UpdateMode& out);
const char* updateStateName(UpdateState s);

// Compares semver versions ("v1.2.3" or "1.2.3", optionally with a pre-release such as "-beta.1"):
// 1.4.0-beta.1 < 1.4.0-beta.2 < 1.4.0-rc.1 < 1.4.0. Returns <0, 0 or >0. Invalid = 0.0.0.
int compareVersions(const char* a, const char* b);

// Ring along the edge of the round screen plus text, over the face.
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
