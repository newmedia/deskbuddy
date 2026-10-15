#pragma once
// Bluetooth LE: Nordic UART Service met hetzelfde commandoprotocol als serial/web.
// Koppelen gaat met een pincode op het scherm; zonder koppeling geen toegang.

#include <stdint.h>

namespace buddy {
class App;
struct DeviceInfo;
}  // namespace buddy

namespace ble {

void begin(buddy::App& app, bool enabled, const char* name);
void loop();
void setEnabled(bool on);
void forgetBonds();
void fill(buddy::DeviceInfo& d);

}  // namespace ble
