# Plan: deskbuddy firmware and apps

The original firmware is closed source and therefore cannot be modified. That is why we are
building custom deskbuddy firmware from scratch on the same hardware (see `hardware.md`). The
backup of the original firmware is in `backups/`; restoring it is always possible.

## Chosen starting points (2026-10-15)

| Topic | Choice |
|---|---|
| Firmware | PlatformIO + Arduino-ESP32 3.x + **LVGL 9** |
| Animations | **Mix**: a procedurally drawn face with eyes and emotions as the basis, plus sprite animations for special moments |
| Control | Web interface on the device, BLE, Windows app and Home Assistant/MQTT |
| Phone | **PWA**: the same web app talks over Wi-Fi or Web Bluetooth. Web Bluetooth does not work in Safari on iOS; there it goes over Wi-Fi. |
| Desktop | **Tauri** (Rust + the same web UI), for Windows and also Linux |
| Features | Clock, weather and calendar; reacting to movement and touch; sound and speech |
| Flash | Fully for deskbuddy, with two OTA slots and no fallback to the original firmware on the device |

## Architecture

```
            ┌───────────── deskbuddy (ESP32-S3) ─────────────┐
            │  Face engine (LVGL 9)  ←  Behaviour/emotion FSM│
            │      ↑ sprites (LittleFS)      ↑ events        │
            │  Sensors: touch · IMU · RTC · PMU · mic        │
            │  Audio: ES8311 speaker · ES7210 mic            │
            │  Command bus (JSON) ← HTTP/WebSocket · BLE GATT · MQTT
            └───────────────┬───────────────┬────────────────┘
                 Wi-Fi (mDNS deskbuddy.local)│BLE
        ┌───────────────────┴──┐   ┌────────┴─────────┐   ┌──────────────┐
        │ Web UI / PWA         │   │ Tauri desktop app│   │ Home Assistant│
        │ (served by the ESP)  │   │ (PC status push) │   │ via MQTT disc.│
        └──────────────────────┘   └──────────────────┘   └──────────────┘
```

- **One command protocol** (JSON messages such as `{"cmd":"emotion","value":"happy"}`) that works
  the same over WebSocket, BLE (GATT characteristic) and MQTT. All clients speak the same language.
- **Web UI** is built as static files and served from LittleFS. The PWA and the Tauri app use the
  same code.
- **Wi-Fi setup**: first via BLE from the PWA, or via an access point with a captive portal as a
  fallback.
- **OTA**: update firmware and LittleFS via the web interface or the desktop app.

## Repo layout (proposal)

```
firmware/        PlatformIO project (src/, lib/, data/ for LittleFS)
  src/face/      procedural face, emotions, sprite player
  src/drivers/   display (CO5300 QSPI), touch, IMU, PMU, RTC, audio
  src/net/       wifi, webserver/ws, ble, mqtt, ota
web/             web UI + PWA (Vite + TypeScript); the build goes to firmware/data/
desktop/         Tauri app (reuses web/)
protocol/        JSON schema of the commands and events (shared)
docs/
tools/
```

## Phases

Navigation (fixed): **swipe down** opens the info panel with Wi-Fi (IP/SSID or AP), BLE and battery.
**Swipe up** opens the clock. Wi-Fi and BLE are optional; without a connection the buddy works
offline, but with fewer features.

1. ✅ **Base firmware** (see `firmware.md`): the display works with LVGL 9 at 466×466. The procedural face blinks,
   looks around and has 6–8 emotions. Touch gestures (tap, stroke, long press) and IMU events
   (shake, pick up, tilt) trigger reactions. Plus clock via the RTC and the battery indicator.
2. ✅ **Wi-Fi and web** (QR setup, captive portal, `deskbuddy.local`, web UI, OTA, NTP): captive portal setup, mDNS `deskbuddy.local`, web server with WebSocket,
   web UI with buttons for emotion, message, brightness and volume, NTP time and OTA.
   → *This is the MVP.* ✅ Plus automatic updates via GitHub Releases (`tools/release.sh`).
3. ✅ **BLE and PWA** (NUS with PIN pairing, PWA on GitHub Pages via Web Bluetooth): GATT service with the same protocol, Wi-Fi provisioning via BLE and an
   installable PWA.
4. **Desktop app (Tauri)**: connects via mDNS or BLE and pushes PC status, notifications and
   calendar items to the buddy.
5. **Home Assistant/MQTT**: MQTT discovery, emotions as `select`, sensors for battery and
   motion, and showing HA values on the screen.
6. **Clock, weather and calendar**: weather via Open-Meteo (no API key needed). The calendar goes
   through the desktop app or HA.
7. **Sound and speech** (✅ sound effects via the synth, on/off in the info panel): sound effects via the ES8311, clap and voice detection via the ES7210, and
   possibly speech interaction with an LLM later via the desktop app or HA.
8. ✅ **Skins**: `classic` (robot) and `mascot` (cartoon with hair, glasses and a smile). Possibly
   more skins later.
9. **Sprite animations**: special animations from LittleFS (for example an LVGL image sequence) for
   events such as a birthday or a new notification.

## Risks and points of attention
- **Performance**: 466×466 at 16-bit is ±434 KB per frame. That calls for LVGL buffers in PSRAM,
  partial updates and DMA over QSPI. Aim for 30+ fps for the face.
- Wi-Fi and BLE at the same time is possible on the S3, but costs RAM. That is why we use NimBLE
  instead of Bluedroid.
- Flashing requires permission (see `CLAUDE.md`). The first flash overwrites the original
  firmware; the backup is ready.

## Multiple buddies (idea)

Reacting to each other and doing things together (contagious behavior, choir, wave, messages to colleagues):
see `multi-buddy.md` for the design proposal (ESP-NOW locally + MQTT remotely).

## Animations (✅ whistle, wink, kiss, rage and explode built; the rest is still open)

Special animations are *actions*: a short timeline on top of the regular emotion. They get their own
sound and then return to the previous emotion. They work in both skins and can be started via
a command (`play <name>`), the web interface, BLE and HA, and some also via gestures.

| Action | Visual | Sound | Trigger |
|---|---|---|---|
| Whistle | pursed mouth, music notes float upward, eyes half closed | little tune (synth) | spontaneously when bored |
| Wink | one eye closed (separate eyelid per eye), slight tilt of the head | "ting" | double tap on one eye |
| Kiss | pursed lips, a heart flies forward and grows | smack | long stroking |
| Angry | red tint, eyebrows/eyelids slanted, short shake | growl | repeated tapping |
| Rage | deep red, screen shakes, steam clouds from the sides, "#@!" symbols | snarling + hissing | even more tapping or shaking when already angry |
| Explode | swelling, white flash, particles fly away, then charred and surprised (black streaks, smoke plume) | "boom" (noise burst) | rage that keeps going |
| Sneeze | eyes closed, build-up "ah-ah-", sneeze jolt | "achoo" | spontaneously |
| Laugh | eyes "^ ^", whole face shakes rhythmically | "hehehe" chirps | tickling (fast stroking) |
| Eye roll | eyes roll a full circle upward | sigh | swipe back and forth |

Needed in the engine: per-eye overrides (wink), a particle system (notes, hearts,
steam, explosion), screen shake (offset on everything) and tint/flash overlays, plus an `Action`
timeline with keyframes per parameter. The synth gets noise (explosion, hissing) and melodies (whistling).
