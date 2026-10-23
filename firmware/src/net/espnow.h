#pragma once
// ESP-NOW as the transport for buddies nearby (lib/buddy/src/mesh.h, docs/multi-buddy.md): broadcast
// on the current Wi-Fi channel. Only on while the App is in a group (or scanning) and Wi-Fi is on.

#include <stddef.h>
#include <stdint.h>

namespace buddy {
class App;
}

namespace espnow {

void begin(buddy::App& app);
// Main loop: (re)starts or stops ESP-NOW when the group or Wi-Fi changes, and hands received frames
// to the App (the receive callback runs in the Wi-Fi task and only fills a queue).
void loop();
void send(const uint8_t* data, size_t len);

}  // namespace espnow
