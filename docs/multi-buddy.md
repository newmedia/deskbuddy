# Multiple deskbuddies: reacting to each other and doing things together

Status: **prototype** (2026-10-23): seeing each other and a contagious yawn over ESP-NOW, with signed
messages, groups with an invite and searching for groups (see "Prototype" below and `firmware.md`).
The rest is still a design proposal. This document describes what is possible, which technology fits, and the order of work.

## What do we want?

A few examples, from simple to ambitious:

| Idea | What you see |
|---|---|
| **Seeing each other** | The info panel shows which buddies are nearby ("3 buddies nearby"). |
| **Contagious behavior** | One buddy yawns, and a moment later the others yawn too. If one laughs, the neighbors laugh along. |
| **Looking at whoever does something** | When a buddy explodes or sneezes, the other eyes look in that direction. |
| **Chain reaction** | When one buddy throws a fit, the others get scared, and then they explode one by one. |
| **The wave** | Buddies in a row jump up one after another, from left to right. |
| **Choir** | Whistling together: each buddy plays a different voice of the same chord, exactly at the same time. |
| **Group pomodoro** | Everyone focuses at once; at the same moment all buddies get a break animation. |
| **Sending a message** | A tap on your buddy sends a heart or a kiss to a colleague's buddy. |
| **Celebrating together** | A successful deploy or a birthday (via Home Assistant or a webhook) makes all buddies party at once. |

## Technology: how do they talk to each other?

| Option | Range | Speed | Pros | Cons |
|---|---|---|---|---|
| **ESP-NOW** (direct radio, ESP32 ↔ ESP32) | same room/floor (same radio as Wi-Fi) | ~1–5 ms | no router needed; works alongside Wi-Fi; broadcast; little RAM | only nearby; all buddies on the same Wi-Fi channel; broadcast cannot use ESP-NOW's own encryption; max 1470 bytes per message (v2) |
| **Wi-Fi LAN** (mDNS + UDP multicast) | same network | ~5–50 ms | discovery via `_deskbuddy._tcp`; multiple rooms | only on the same (home/office) network; guest networks often block multicast |
| **MQTT broker** (e.g. Home Assistant or a cloud broker) | anywhere with internet | ~50–300 ms | buddies in different locations; fits phase 5 (HA) | broker needed; TLS costs internal RAM |
| **BLE** | ~10 m | slow (scanning) | estimate distance via RSSI ("who is standing next to me?") | scanning needs the observer role, which we turned off to save RAM |

**Proposal:** combine two layers with the **same message format**:

1. **ESP-NOW** for everything in the same room. It is fast enough for chain reactions and
   simultaneous actions, and also works when a buddy has no Wi-Fi.
2. **MQTT** for remote buddies, such as messages to colleagues or a group party via Home
   Assistant.

## ESP-NOW in detail (researched 2026-10-23)

Sources: our own framework (Arduino-ESP32 3.3.11 on ESP-IDF 5.5: `esp_now.h`, the build's `sdkconfig`
and the bundled `ESP_NOW` library with broadcast and network examples) and Espressif's
[ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html)
and [RF coexistence](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/coexist.html)
documentation for the ESP32-S3.

ESP-NOW sends short Wi-Fi frames straight from one ESP32 to another (or to everyone: broadcast), on
the channel the radio is on at that moment, by default at 1 Mbps. No router, no connection set-up.

| Topic | What applies to us |
|---|---|
| Message size | **ESP-NOW v2: up to 1470 bytes** (`ESP_NOW_MAX_DATA_LEN_V2`); v1: 250. Our framework has v2. A v1 receiver truncates or drops longer messages, but all buddies run our firmware. Small messages are still better: less airtime at 1 Mbps. |
| Peers | At most **20** registered peers. Broadcast needs the broadcast address as a peer; receiving from anyone works without registering. |
| Encryption | CCMP with a 16-byte PMK and per-peer LMK, **only for unicast**: "encrypting multicast vendor-specific action frame is not supported". At most **7 encrypted peers** in our build (`CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=7`, compiled into the precompiled IDF libraries). |
| Delivery | A successful send only means "received at the MAC layer", not that the app got it. Espressif advises application-level acknowledgements, retries and sequence numbers against duplicates. Broadcast frames get no per-receiver confirmation. |
| Channel | Sender and receiver must be on the **same channel**. A buddy connected to a router is on the router's channel and cannot change it. |
| Callbacks | Send and receive callbacks run in the high-priority Wi-Fi task: copy the data into a queue and handle it in the main loop (LVGL is not thread-safe anyway). The received info is only valid inside the callback. |
| Power saving | By default ESP-NOW keeps listening (wake window at its maximum). Our firmware turns modem sleep off when connected (`WiFi.setSleep(false)` in `net.cpp`), so reception is not cut into slices by power saving. |
| Together with Bluetooth (ESP32-S3 table) | **Sending is stable** in every BLE state. **Receiving is "stable in STA mode, otherwise not supported"**: a buddy in setup-network mode (SoftAP, `WIFI_AP_STA`) or with Wi-Fi off cannot reliably receive while BLE is on. With Wi-Fi and BLE both connected, each gets half of the radio time. |
| Range | Same radio and antenna as Wi-Fi, so roughly what Wi-Fi does on that board. No own measurement yet. The XL currently has a much weaker signal (antenna, see `xl-port.md`), which limits ESP-NOW on it too. |

### What this means for deskbuddy

1. **The channel is the main design issue.** Buddies on the same router are automatically on the same
   channel. A buddy without home Wi-Fi has to find the group: run Wi-Fi in STA mode without a
   connection and go over channels 1–13 with a `hello` until a buddy answers, then stay there. In an
   office with several access points on different channels, buddies on different APs cannot hear
   each other; that is where the MQTT/LAN layer comes in.
2. **Setup mode does not count.** In the setup network (SoftAP) reception with BLE on is not
   supported. That is fine: a buddy being set up does not need to join in yet.
3. **Group security in the app, not in ESP-NOW.** Broadcast cannot be encrypted, and unicast
   encryption stops at 7 peers. What we actually need is authenticity ("a stranger's buddy cannot
   make mine explode"), not secrecy: a group key with an HMAC over each message (mbedTLS is already
   in the firmware), plus `seq` and time against replays. Emotions and actions are not secret.
4. **Message format stays as proposed** (small JSON). With 1470 bytes there is room, but keep
   `hello`/`event` well under 250 bytes for airtime.
5. **Structure in the firmware:** receive callback → queue (PSRAM) → `net::loop()` → `App`, just like
   the BLE and HTTP commands. Internal RAM is tight (~80 KB free); ESP-NOW reuses the Wi-Fi buffers,
   but measure it in the first prototype with Wi-Fi, BLE and ESP-NOW all on.
6. **Testable without hardware:** give the web emulator a fake ESP-NOW over UDP on localhost, so two
   or three emulators can see each other in the browser and in the regression test.

### First step (prototype)

Two buddies on the same router: a `hello` broadcast every few seconds with an HMAC over the group
key, a "buddies nearby" list in `json` and the info panel, and a contagious yawn. Measure: internal
RAM, how many `hello`s arrive with BLE connected, and the delay from event to reaction.

### Prototype (built 2026-10-23)

- **Groups:** a random 128-bit key plus a name. Create one in the web app (Buddies tab) or with
  `group create <name>`; others join with the invite (QR on the buddy's screen, a link, or the text).
- **Groups nearby:** a visible group sends its name and a public id every 5 s; searching lists them.
  The name is only a label, the invite is needed to join. Visibility can be turned off.
- `hello` every 3 s, list of up to 8 buddies (gone after 10 s), "N buddies nearby" in the info panel.
- Signed frames as proposed below (truncated HMAC-SHA256 over the group key); boot id + sequence
  number against replays (a replay of an old boot is not caught yet).
- Contagious yawn with a random delay, a 30 s cooldown and no passing on.
- Device: ESP-NOW broadcast on the current channel (`src/net/espnow.cpp`); web emulator: UDP on
  localhost; the regression test runs two emulators against each other.
- **Security notes:** whoever has the invite has the key (like a Wi-Fi password); there is no way to
  remove one member except making a new group. The buddy's web API has no login on the LAN, so
  anyone on that network can read the invite with `group invite` (as with every other setting).
  Name beacons are unsigned: anyone can announce a fake group name, but cannot join a real group.
- **Next step:** joining by confirming on a group member's screen (key exchange plus a short code
  shown on both screens, like Bluetooth pairing), so no invite has to be passed on.
- Not done yet: channel search for buddies without home Wi-Fi, "look at whoever does something",
  synchronized actions, a measurement on two real buddies (RAM, delivery with BLE on, delay).

## Messages (draft)

Small JSON, matching the existing commands. A buddy that does something *reports* it (event). A
buddy that has to do something receives a *request* (cmd).

```json
{"v":1,"t":"event","from":"deskbuddy-A45C","name":"action","value":"laugh","seq":812}
{"v":1,"t":"cmd","from":"deskbuddy-A45C","to":"*","cmd":"play wave","at":1792101234567,"group":"team"}
{"v":1,"t":"hello","from":"deskbuddy-B21F","skin":"mascot","pos":{"x":2,"y":0},"name":"Bob"}
```

- **`hello`** (every few seconds): I am here, with name, skin and position. This is how every buddy
  knows who is nearby.
- **`event`**: what happened (emotion, action, gesture). The others decide for themselves whether
  and how to react.
- **`cmd` with `at`**: a joint action at an agreed time. This keeps choir, wave and party in sync.

## Key design decisions

- **Groups and trust:** buddies only react to buddies from their own group ("team"). A group has a
  random key; joining is done with an invite (QR on the buddy, link or text). Messages carry an HMAC
  with the group key (ESP-NOW cannot encrypt broadcast, see above), so a stranger's buddy cannot
  simply make your buddy explode.
- **No endless chain reactions:** every event gets a `seq`, a hop count and a cooldown. Laughter is
  contagious, but not back and forth forever.
- **Personality:** every buddy gets a few settings, such as how contagious, how quickly annoyed and
  whether it wants to take part in group actions. This way not every buddy reacts the same, which
  makes it livelier.
- **Time synchronization:** via NTP the clocks already agree to within a few ms. For ESP-NOW groups
  without internet, a "leader" sends its time along, and the others correct their offset.
- **Spatial awareness** (for "look at whoever does something" and the wave): manual for now. In the
  web interface you drag the buddies onto a desk map (left/right). Later possibly an estimate
  via BLE signal strength.
- **Memory and power:** ESP-NOW is light: it reuses the Wi-Fi buffers (to be measured). MQTT with TLS is heavier, but mbedTLS already
  runs in PSRAM. The radio is on for Wi-Fi anyway, so the extra power is limited.

## Proposed order

1. **Seeing each other:** ESP-NOW `hello`, a "buddies nearby" list in the info panel and the web
   interface, and pairing groups with a code.
2. **Contagious behavior:** yawning, laughing and getting scared; looking at whoever does something
   (using the desk map).
3. **Doing things together:** synchronized actions with `at`, such as choir, wave, party and group
   pomodoro.
4. **Remote:** MQTT bridge, so you can send messages to colleagues and start group parties via Home
   Assistant.
5. **Smarter:** personalities, and buddies that make "agreements" (e.g. staying quiet together for
   fifteen minutes when someone is in a call).

## Open questions

- ~~One room or spread out?~~ **One room** (2026-10-16): so ESP-NOW first; MQTT only later for
  remote buddies. Each buddy can get its own look (see "Appearance" in `firmware.md`).
- Who may control a buddy: only buddies from your own group, or also "public" messages?
- Should a group action always be allowed, or do you want a per-buddy "do not disturb" setting?
