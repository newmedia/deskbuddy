#pragma once
// Bluetooth LE: Nordic UART Service with the same command protocol as serial/web.
// Pairing uses a passkey on the screen; without pairing there is no access.

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
// Temporarily invisible and without connections (e.g. during a firmware update). The controller
// stays on: shutting it down alongside Wi-Fi left the device hanging. Returns false if BLE wasn't running.
bool suspend();
void resume();
bool isConnected();

}  // namespace ble
