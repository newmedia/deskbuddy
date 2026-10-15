#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace buddy {

namespace {
Gesture gestureFromName(const char* n) {
  for (int i = 1; i <= (int)Gesture::PickUp; i++)
    if (strcmp(n, gestureName((Gesture)i)) == 0) return (Gesture)i;
  return Gesture::None;
}
}  // namespace

bool runCommand(const char* line, Face& face, Behaviour& behaviour, CommandHost& host, char* reply, size_t len) {
  char buf[160];
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  char* save = nullptr;
  char* cmd = strtok_r(buf, " \t\r\n", &save);
  char* a1 = strtok_r(nullptr, " \t\r\n", &save);
  char* a2 = strtok_r(nullptr, " \t\r\n", &save);
  if (!cmd) {
    reply[0] = 0;
    return true;
  }

  if (!strcmp(cmd, "emotion") && a1) {
    Emotion e;
    if (!emotionFromName(a1, e)) {
      snprintf(reply, len, "onbekende emotie: %s", a1);
      return false;
    }
    behaviour.command(e, a2 ? (uint32_t)atoi(a2) : 0);
    snprintf(reply, len, "ok emotion=%s", emotionName(e));
  } else if (!strcmp(cmd, "play") && a1) {
    Action a;
    if (!actionFromName(a1, a)) {
      snprintf(reply, len, "gebruik: play whistle|wink|kiss|rage|explode");
      return false;
    }
    face.play(a);
    snprintf(reply, len, "ok play=%s", a1);
  } else if (!strcmp(cmd, "look") && a1) {
    if (!strcmp(a1, "off")) face.releaseLook();
    else face.lookAt((float)atof(a1), a2 ? (float)atof(a2) : 0.f, true);
    snprintf(reply, len, "ok");
  } else if (!strcmp(cmd, "blink")) {
    face.blink();
    snprintf(reply, len, "ok");
  } else if (!strcmp(cmd, "bounce")) {
    face.bounce(1.0f);
    snprintf(reply, len, "ok");
  } else if (!strcmp(cmd, "gesture") && a1) {
    GestureEvent g;
    g.type = gestureFromName(a1);
    if (g.type == Gesture::None) {
      snprintf(reply, len, "onbekend gebaar: %s", a1);
      return false;
    }
    behaviour.onGesture(g);
    snprintf(reply, len, "ok gesture=%s", a1);
  } else if (!strcmp(cmd, "brightness") && a1) {
    int v = LV_CLAMP(0, atoi(a1), 255);
    host.setBrightness((uint8_t)v);
    snprintf(reply, len, "ok brightness=%d", v);
  } else if (!strcmp(cmd, "clock")) {
    host.showClock();
    snprintf(reply, len, "ok");
  } else if (!strcmp(cmd, "info")) {
    host.showInfo();
    snprintf(reply, len, "ok");
  } else if (!strcmp(cmd, "sound") && a1) {
    Sound s;
    if (!strcmp(a1, "on") || !strcmp(a1, "off")) {
      host.setSound(!strcmp(a1, "on"));
    } else if (soundFromName(a1, s)) {
      host.playSound(s);
    } else {
      snprintf(reply, len, "gebruik: sound on|off|<naam>");
      return false;
    }
    snprintf(reply, len, "ok sound %s", a1);
  } else if (!strcmp(cmd, "skin") && a1) {
    Skin s;
    if (!skinFromName(a1, s)) {
      snprintf(reply, len, "gebruik: skin classic|mascot");
      return false;
    }
    host.setSkin(s);
    snprintf(reply, len, "ok skin=%s", a1);
  } else if (!strcmp(cmd, "wifi")) {
    // wifi status | setup | off | join <ssid> <wachtwoord>   (ssid zonder spaties; wachtwoord = rest van de regel)
    const char* action = a1 ? a1 : "status";
    const char* pass = "";
    if (!strcmp(action, "join")) {
      char* rest = strtok_r(nullptr, "\r\n", &save);
      pass = rest ? rest : "";
      if (!a2) {
        snprintf(reply, len, "gebruik: wifi join <ssid> <wachtwoord>");
        return false;
      }
    }
    host.wifi(action, a2 ? a2 : "", pass, reply, len);
  } else if (!strcmp(cmd, "update")) {
    const char* action = a1 ? a1 : "status";
    if (strcmp(action, "status") && strcmp(action, "check") && strcmp(action, "install") && strcmp(action, "auto") &&
        strcmp(action, "notify") && strcmp(action, "off")) {
      snprintf(reply, len, "gebruik: update [status|check|install|auto|notify|off]");
      return false;
    }
    host.firmwareUpdate(action, reply, len);
  } else if (!strcmp(cmd, "sleep") && a1) {
    int m = !strcmp(a1, "off") || !strcmp(a1, "never") ? 0 : LV_CLAMP(0, atoi(a1), 120);
    host.setSleep((uint8_t)m);
    if (m) snprintf(reply, len, "ok slapen na %d min", m);
    else snprintf(reply, len, "ok slapen uit");
  } else if (!strcmp(cmd, "volume") && a1) {
    int v = LV_CLAMP(0, atoi(a1), 100);
    host.setVolume((uint8_t)v);
    snprintf(reply, len, "ok volume=%d", v);
  } else if (!strcmp(cmd, "status")) {
    char extra[224];
    host.status(extra, sizeof(extra));
    snprintf(reply, len, "emotion=%s idle=%lus %s", emotionName(face.emotion()),
             (unsigned long)(behaviour.idleMs() / 1000), extra);
  } else if (!strcmp(cmd, "help")) {
    snprintf(reply, len,
             "emotion <naam> [ms] | play whistle|wink|kiss|rage|explode | look <x> <y>|off | blink | bounce | gesture <naam> | brightness <0-255> | "
             "clock | info | sound on|off|<naam> | volume <0-100> | skin classic|mascot | sleep <min>|off | wifi status|setup|off|join <ssid> <pw> | "
             "update check|install|auto|notify|off | status");
  } else {
    snprintf(reply, len, "onbekend commando (probeer 'help')");
    return false;
  }
  return true;
}

}  // namespace buddy
