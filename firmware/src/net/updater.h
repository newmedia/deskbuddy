#pragma once
// Automatische firmware-updates via GitHub Releases (newmedia/deskbuddy).
// Bij opstarten en elke 12 uur: releases/latest opvragen, tag vergelijken met DESKBUDDY_VERSION en
// afhankelijk van de instelling (auto|notify|off) melden of meteen firmware.bin installeren.
// Opvragen en installeren lopen in een eigen taak, zodat het gezicht blijft bewegen.

#include <stddef.h>

namespace buddy {
struct DeviceInfo;
}

namespace net::updater {

void begin(const char* currentVersion);  // laadt de instelling uit NVS
void loop(bool online);                  // planning; online = verbonden met het thuisnetwerk
void fill(buddy::DeviceInfo& d);
// action: status | check | install | auto | notify | off
void action(const char* action, char* reply, size_t len);

}  // namespace net::updater
