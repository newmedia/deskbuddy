#pragma once
// Text commands ("emotion happy 3000", "look 0.5 -0.2", ...). Now used via serial and
// later via WebSocket/BLE/MQTT (then translated as JSON into the same calls).

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "behaviour.h"
#include "face.h"
#include "sound.h"

namespace buddy {

struct CommandHost {
  virtual ~CommandHost() = default;
  virtual void setBrightness(uint8_t) {}
  virtual void showClock() {}
  virtual void showInfo() {}
  virtual void setSound(bool) {}
  virtual void setVolume(uint8_t) {}
  virtual void playSound(Sound) {}
  virtual void setSkin(Skin) {}
  virtual void setSleep(uint8_t) {}
  virtual void ble(const char* action, char* reply, size_t len) {
    (void)action;
    snprintf(reply, len, "ble not available");
  }
  // action: status | setup | off | join (then ssid + pass)
  virtual void wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) {
    (void)action; (void)ssid; (void)pass;
    snprintf(reply, len, "wifi not available");
  }
  // action: status | check | install | auto | notify | off
  virtual void firmwareUpdate(const char* action, char* reply, size_t len) {
    (void)action;
    snprintf(reply, len, "updates not available");
  }
  virtual void status(char* out, size_t len) { (void)len; out[0] = 0; }
};

// Executes one line; writes the reply into reply. Returns false for an unknown command.
bool runCommand(const char* line, Face& face, Behaviour& behaviour, CommandHost& host, char* reply, size_t len);

}  // namespace buddy
