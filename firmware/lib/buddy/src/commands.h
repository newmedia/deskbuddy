#pragma once
// Tekstcommando's ("emotion happy 3000", "look 0.5 -0.2", ...). Wordt nu via serial gebruikt en
// later via WebSocket/BLE/MQTT (dan als JSON vertaald naar dezelfde aanroepen).

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
  // action: status | setup | off | join (dan ssid + pass)
  virtual void wifi(const char* action, const char* ssid, const char* pass, char* reply, size_t len) {
    (void)action; (void)ssid; (void)pass;
    snprintf(reply, len, "wifi niet beschikbaar");
  }
  virtual void status(char* out, size_t len) { (void)len; out[0] = 0; }
};

// Voert één regel uit; schrijft antwoord in reply. Geeft false bij een onbekend commando.
bool runCommand(const char* line, Face& face, Behaviour& behaviour, CommandHost& host, char* reply, size_t len);

}  // namespace buddy
