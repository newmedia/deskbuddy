#pragma once
// Netwerk: Wi-Fi (station + setup-access-point met captive portal), mDNS, NTP, web-API en OTA.
// Alles is optioneel: zonder Wi-Fi blijft de buddy gewoon offline werken.

#include <stddef.h>

namespace buddy {
class App;
struct DeviceInfo;
}  // namespace buddy

namespace net {

void begin(buddy::App& app);
void loop();
void fill(buddy::DeviceInfo& d);

void startSetupAp();
void off();
void join(const char* ssid, const char* pass);
void action(const char* action, const char* ssid, const char* pass, char* reply, size_t len);

}  // namespace net
