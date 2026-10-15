#pragma once
// Automatische firmware-updates via GitHub Releases (newmedia/deskbuddy).
// Bij opstarten en elke 12 uur: releases/latest opvragen, tag vergelijken met DESKBUDDY_VERSION en
// afhankelijk van de instelling (auto|notify|off) melden of meteen firmware.bin installeren.
// Controleren loopt in een eigen taak (stack in PSRAM); installeren op de hoofdlus, met pump()
// zodat het gezicht en de voortgangsring blijven bewegen.

#include <stddef.h>

namespace buddy {
struct DeviceInfo;
}

namespace net::updater {

void begin(const char* currentVersion);  // laadt de instelling uit NVS
// Wordt tijdens het installeren (blokkerend, op de hoofdlus) regelmatig aangeroepen om het scherm
// bij te werken.
void setPump(void (*pump)());
void loop(bool online);
// Rond een netwerkklus: prepare() maakt geheugen vrij (BLE tijdelijk uit) en mag een automatische
// controle uitstellen (urgent = false) door false te geven; release() zet alles terug.
void setHooks(bool (*prepare)(bool urgent), void (*release)());                  // planning; online = verbonden met het thuisnetwerk
void fill(buddy::DeviceInfo& d);
// action: status | check | install | auto | notify | off
void action(const char* action, char* reply, size_t len);

}  // namespace net::updater
