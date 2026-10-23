#pragma once
// Network: Wi-Fi (station + setup access point with captive portal), mDNS, NTP, web API and OTA.
// Everything is optional: without Wi-Fi the buddy simply keeps working offline.

#include <stddef.h>

namespace buddy {
class App;
struct DeviceInfo;
}  // namespace buddy

namespace net {

void begin(buddy::App& app);
void loop();
void setPump(void (*pump)());  // called during web/ArduinoOTA upload to update the screen
void fill(buddy::DeviceInfo& d);
// Plain malloc from 256 bytes up goes to PSRAM (more free internal RAM). During a firmware update
// switch back to internal: writing flash from PSRAM buffers and keeping network buffers in PSRAM make
// the upload many times slower.
void updateMemory(bool updating);

void startSetupAp();
void off();
void join(const char* ssid, const char* pass);
void action(const char* action, const char* ssid, const char* pass, char* reply, size_t len);

}  // namespace net
