# Multiple deskbuddies: reacting to each other and doing things together

Status: **idea / design proposal** (2026-10-16). Nothing built yet. This document describes what is
possible, which technology fits, and a proposed order of work.

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
| **ESP-NOW** (direct radio, ESP32 ↔ ESP32) | same room/floor, ~10–50 m | ~1–5 ms | no router needed; works alongside Wi-Fi; broadcast; very little RAM | only nearby; needs the same Wi-Fi channel as the router; messages max ~250 bytes |
| **Wi-Fi LAN** (mDNS + UDP multicast) | same network | ~5–50 ms | discovery via `_deskbuddy._tcp`; multiple rooms | only on the same (home/office) network; guest networks often block multicast |
| **MQTT broker** (e.g. Home Assistant or a cloud broker) | anywhere with internet | ~50–300 ms | buddies in different locations; fits phase 5 (HA) | broker needed; TLS costs internal RAM |
| **BLE** | ~10 m | slow (scanning) | estimate distance via RSSI ("who is standing next to me?") | scanning needs the observer role, which we turned off to save RAM |

**Proposal:** combine two layers with the **same message format**:

1. **ESP-NOW** for everything in the same room. It is fast enough for chain reactions and
   simultaneous actions, and also works when a buddy has no Wi-Fi.
2. **MQTT** for remote buddies, such as messages to colleagues or a group party via Home
   Assistant.

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

- **Groups and trust:** buddies only react to buddies from their own group ("team"). Pairing is done
  through the web interface with a group code or QR. ESP-NOW messages are encrypted with the group
  key, so a stranger's buddy cannot simply make your buddy explode.
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
- **Memory and power:** ESP-NOW is light (a few KB). MQTT with TLS is heavier, but mbedTLS already
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
