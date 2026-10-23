#pragma once
// Automatic firmware updates via GitHub Releases (newmedia/deskbuddy).
// At startup and every 12 hours: fetch releases/latest, compare the tag with DESKBUDDY_VERSION and,
// depending on the setting (auto|notify|off), report it or install firmware.bin right away.
// Checking runs in its own task (stack in PSRAM); installing on the main loop, with pump()
// so the face and the progress ring keep moving.

#include <stddef.h>

namespace buddy {
struct DeviceInfo;
}

namespace net::updater {

void begin(const char* currentVersion);  // loads the setting from NVS
// Called regularly during installation (blocking, on the main loop) to update the screen.
void setPump(void (*pump)());
void loop(bool online);
// Around a network job: prepare() frees memory (BLE temporarily off) and may postpone an automatic
// check (urgent = false) by returning false; release() restores everything.
void setHooks(bool (*prepare)(bool urgent), void (*release)());                  // scheduling; online = connected to the home network
void fill(buddy::DeviceInfo& d);
// action: status | check | install | auto | notify | off
void action(const char* action, char* reply, size_t len);

}  // namespace net::updater
