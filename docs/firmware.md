# deskbuddy firmware

PlatformIO project in `firmware/`: Arduino-ESP32 3.3 (pioarduino `55.03.311`) and LVGL 9.5.

## Controls

| Action | Reaction |
|---|---|
| Tap | looks at your finger, happy or surprised; wakes it up |
| Double tap | wink |
| Long press | in love (heart eyes) |
| Stroke (swipe back and forth) | happy, after 3× in love, after 5× a kiss |
| Keep tapping fast | 5× angry, 9× rage, and tapping on during rage makes it explode |
| Swipe left/right | looks in that direction |
| **Swipe down** | **info panel**: Wi-Fi (IP/SSID or access point), BLE, sound and look (tap to switch), battery and version |
| **Swipe up** | **clock** with date and battery ring |
| Tap or swipe back in a panel | close the panel (it also closes by itself) |
| Shake | dizzy, then briefly angry (shaking while angry: rage) |
| Pick up | surprised |
| Tilt | eyes look "downhill" |
| Do nothing | sleepy after 90 s and asleep after 3 min (screen dims) |
| PWR button (short) | info panel on/off |
| BOOT button | cycle through emotions (for testing) |

Wi-Fi and BLE are optional. Without a connection everything above works as usual, and the info
panel shows "not connected" and "off". BLE follows in phase 3.

## Wi-Fi and web interface (phase 2)

1. Without a known network it starts the open setup network **`deskbuddy-XXXX`** (the last 4
   characters of the MAC address). After 10 minutes without visitors it turns off again.
2. Swipe down and tap the Wi-Fi row. **Step 1:** scan the QR code to connect to the setup network.
   **Step 2:** once your phone is connected, the screen shows a second QR code with the link
   `http://192.168.4.1`. On many phones the setup page also opens by itself (captive portal);
   Samsung does not always do that, and step 2 works everywhere.
3. Choose your network (2.4 GHz only), enter the password (with "Show password" you can check it)
   and tap Connect. Under the button you see the progress, the IP address or the reason for a
   failed attempt.
4. 30 seconds after a successful connection it switches over to your home network. There it is
   reachable via the IP from the info panel, or via **`http://deskbuddy.local`** if your device
   supports mDNS.

The web interface offers:
- emotions, blinking, bouncing and shaking
- skin, sound, volume and brightness
- status and Wi-Fi (scan, connect, forget)
- a firmware upload
- a command line

| API | Purpose |
|---|---|
| `GET /api/status` | status as JSON |
| `POST /api/cmd` (`c=<command>`) | any serial command, reply as JSON |
| `GET /api/wifi/scan` | scan for networks (asynchronous: repeat until `scanning:false`) |
| `POST /api/wifi` (`ssid`, `pass`) | connect and save |
| `GET /api/wifi/state` | progress: `connecting`/`connected`/`failed` + IP or reason |
| `POST /update` (multipart `firmware`) | update firmware (OTA) |

Updating firmware without a cable works via the web page, or with
`curl -F firmware=@firmware/.pio/build/esp32/firmware.bin http://<ip>/update`,
or with `cd firmware && ../.venv/bin/pio run -e esp32 -t upload --upload-port deskbuddy.local`
(ArduinoOTA). After connecting it fetches the time via NTP and stores it in the RTC.

## Automatic updates (GitHub Releases)

Connected to the home network, the deskbuddy requests
`https://api.github.com/repos/newmedia/deskbuddy/releases/latest` 30 s after connecting and then
every 12 hours (HTTPS with the CA bundle from ESP-IDF). If the tag is newer than its own version
(semver), it fetches the asset for its own board (`firmware.bin` on the Original, `firmware-xl.bin` on
the XL; a release without that asset is skipped) and flashes it with `HTTPUpdate` (the
redirect to GitHub's download server is followed). Checking and installing run in a separate task;
the face keeps moving.

Setting (in NVS, default `notify`), via command or web UI:

| `update …` | effect |
|---|---|
| `notify` | only report: the info panel shows "Update available: vX (tap)" at the bottom, the web UI a button |
| `auto` | install a new version right away |
| `off` | do not check by itself |
| `check` / `install` | check now / install now |
| (empty) | status: mode, state, current and latest version, last error |

While installing, the face looks surprised and a progress ring runs along the edge. Afterwards it
restarts with the new version; if it fails, it looks sad for a moment and tries to check again
after 30 min.

### Installing via the browser (install page)

- **Board and version choice:** first the board (Original or XL), then the version; the newest is
  preselected and the XL only shows versions that have XL firmware. Each version has its own folder
  (`flash/<version>/firmware.bin` + `manifest.json`, and for the XL `firmware-xl.bin` +
  `manifest-xl.json`); bootloader, partition table and boot_app0 are shared (identical for both
  boards). `?board=xl` and `?v=0.3.2` in the URL select directly; the web app links with the right board.
  Test versions (pre-releases) only appear with "Show test versions" (or `?beta`); "(latest)" is
  always the newest regular version.
- **Logo:** `firmware/web/icon.svg` is the deskbuddy logo (cyan ring with eyes and a smile). It is
  used in the web app (header and tab icon), the install page, the README, the boot screen and
  the info panel of the device (`draw::logo()` in `draw_util.h` draws the same shapes). Note:
  custom drawing (`lv_draw_*` with its own descriptor) ignores the object's `opa` style. Anything
  that fades along, such as the boot screen and the info panel, therefore passes the opa on to
  `draw::logo()` itself. The boot screen is deleted afterwards, not merely hidden. The PNG versions
  (`icon-192/512`, `icon-maskable-512`, `apple-touch-icon`) sit next to the svg for a phone's home
  screen.

For anyone putting the firmware on a board for the first time there is a page at
**https://newmedia.github.io/deskbuddy/flash/**. It uses [ESP Web Tools](https://esphome.github.io/esp-web-tools/)
(Web Serial, so Chrome, Edge or Opera on a computer) and flashes over USB.

- It writes four parts: bootloader (0x0), partition table (0x8000), boot_app0 (0xE000) and the app (0x10000).
  The NVS partition (0x9000) is left alone: Wi-Fi and settings are preserved.
- The firmware comes from the newest GitHub release. So after every release you run
  `tools/publish-flash.sh` (fetches `firmware.bin` and, if the release has it, `firmware-xl.bin`,
  checks the sha256 and publishes to `gh-pages`; `DRYRUN=<dir>` builds the site locally instead).
  Bootloader and partition table come from the local build (`.pio/build/esp32`) and do not change.
- `tools/publish-pwa.sh` refreshes only the page itself (`firmware/web/flash/index.html`).
- Updating also works without this page: the buddy fetches new versions from GitHub by itself.

### Making a release

**Version numbers (since v1.0.0, semver):** new features or extensions raise the middle number
(1.1.0, 1.2.0), only bugfixes the last (1.0.1). The first number only goes up for something that
is no longer compatible with older versions (e.g. a different storage format for settings).

We build ourselves, not with GitHub Actions. `tools/release.sh` does everything:

1. At the top of `CHANGELOG.md`, write a section `## v0.2.0` with what is new (that text becomes
   the description of the release on GitHub). Without a section the script stops and shows a draft
   based on the commits since the previous tag. Commit the changelog.
2. `tools/release.sh 0.2.0` checks that the working directory is clean and that the version is newer
   than the last tag, builds `esp32` and `esp32-xl` with `-D DESKBUDDY_VERSION="0.2.0"`, creates and
   pushes tag `v0.2.0` (`git tag -a v0.2.0 && git push origin v0.2.0`) and uses `gh release create` to
   create the release with `firmware.bin`, `firmware-xl.bin` and a `.sha256` for each.
   `tools/release.sh 0.2.0 --original-only` leaves out the XL (e.g. while it is not tested yet).
   **Test versions:** `tools/release.sh 1.4.0-beta.1` (also `-alpha.N`, `-rc.N`) creates a GitHub
   pre-release. Buddies never install it by themselves (the updater follows `releases/latest`, which
   skips pre-releases), and version comparison follows semver (`1.4.0-beta.1` < `1.4.0-rc.1` <
   `1.4.0`), so a buddy on a test version moves on to the next regular version. The "newer than"
   check only looks at regular releases. Testers install it via the install page ("Show test
   versions", `?beta` or a link with `?v=1.4.0-beta.1`) or by uploading the `.bin`.

`gh` must have write access to `newmedia/deskbuddy` via the API (the script checks that).
Pushing over SSH is not enough: if the organization blocks the GitHub CLI app, approve it under
*Organization settings → Third-party access*, or use a fine-grained token with
"Contents: read and write" for this repo (`GH_TOKEN=… tools/release.sh 0.2.0`). If uploading fails
after the tag has already been pushed, a second run with the same version on the same commit
resumes.

Old releases always remain (the script never modifies existing tags or releases), so you can still
install an older version manually: download that `firmware.bin` and upload it via the web UI or
`curl -F firmware=@firmware.bin http://<ip>/update`. Automatically it only goes forward, never
back.

> Security: the web interface and OTA do not have a password yet. Anyone on your (home) network can
> therefore control and update it. A PIN is planned.

The page lives in `firmware/web/index.html`. When building, it is gzip-compressed and embedded in
the firmware (`scripts/embed_web.py`).

## Bluetooth and PWA (phase 3)

- The deskbuddy advertises as **`deskbuddy-XXXX`** with the Nordic UART Service (NUS, `6E400001-…`).
  Commands go to RX (`…0002`, write), and replies come as notify on TX (`…0003`), in chunks and
  terminated with `\n`. The protocol is the same as serial/web, plus `json` for the status.
- **Pairing with a PIN:** on the first connection the screen shows a 6-digit code, which you enter
  on your phone. Only paired devices may send commands. `ble forget` erases all pairings.
- **The PWA** lives at **https://newmedia.github.io/deskbuddy/**. It uses the same page
  (`firmware/web/`), but talks via Web Bluetooth, also works without Wi-Fi and can be installed as
  an app ("Add to home screen"). Via Bluetooth you can also set up Wi-Fi: scan for networks,
  connect.
  - Works in Chrome on Android and desktop. **Not on iPhone**: Safari has no Web Bluetooth. Use
    Wi-Fi via the IP address there.
  - The version that the device serves itself (http) uses Wi-Fi. Web Bluetooth requires https,
    which is why the BLE version lives on GitHub Pages.
- BLE on/off: tap the Bluetooth row in the info panel, or use `ble on|off`. The setting is saved.
- Publishing happens **without GitHub Actions**: `tools/publish-pwa.sh` puts the files from
  `firmware/web/` on the `gh-pages` branch. Run this script after every change to the page.
  Needed once: GitHub → Settings → Pages → Source: **Deploy from a branch**, branch `gh-pages`,
  folder `/ (root)`.

## Actions

Short animations on top of the emotion (`face_fx.cpp`). Afterwards it returns to the previous emotion.

| Action | Command | Visual | Sound |
|---|---|---|---|
| Whistle | `play whistle` | pursed mouth, music notes, half-closed eyes (also spontaneously when bored) | little tune |
| Wink | `play wink` | one eye closed, little star | "ting" |
| Kiss | `play kiss` | eyes closed, pursed lips, a heart flies forward | smack |
| Rage | `play rage` | red glow, shaking, steam clouds, `#@!$%` | growl + hiss |
| Explode | `play explode` | swelling, white flash, sparks, then soot, smoke and dizzy/surprised | fuse hiss + boom |
| Sneeze | `play sneeze` | "ah-ah-": eyes squeeze shut, head tilts back; "achoo!": jolt with droplets (also spontaneously) | inhale + sneeze |
| Laugh | `play laugh` | "^ ^" eyes, rhythmic bouncing (tickling = 3× fast stroking) | "hehehe" |
| Eye roll | `play eyeroll` | gaze rolls in an arc over the top (2× fast swipe back and forth) | sigh |
| Hiccup | `play hiccup` | three sudden jolts with big eyes (also spontaneously) | "hic!" |

Emotions with their own animation (both skins):

| Emotion | Command | What you see |
|---|---|---|
| Crying | `emotion cry` | sad face, tears keep rolling down the cheeks |
| Busy (ADHD) | `emotion hyper` | wide-open eyes, big smile, gaze keeps darting elsewhere, little jumps |
| Pain | `emotion pain` | squeezed-shut eyes, grimace with clenched teeth, sweat drop, an occasional sting (shaking) |

Bouncing (`bounce`) is a little jump: the face goes ~48 px up in an arc and springs back on
landing. The hiccup gives a jolt upward and makes the eyes pop for a moment. The face draws in
fixed screen coordinates; shifting and shaking therefore go through a draw offset (`draw::ox/oy` in
`draw_util.h`). Moving the object did nothing visible. During a jump, a single snapshot of the face
(PSRAM) is shifted instead of redrawing everything every frame.

The engine part contains:
- closing per eye
- a particle system (notes, hearts, steam, smoke, sparks, symbols, stars)
- shaking (the whole face object shifts)
- tint, flash and soot layers

Every action sets *cues*, to which the App attaches a sound. They work in both skins.

## Performance and memory

- **Display:** own driver (`src/board/display.cpp`) via ESP-IDF `esp_lcd`, QSPI at 80 MHz.
  - LVGL draws into a full-screen buffer in PSRAM (434 KB), directly in RGB565_SWAPPED. Every
    changed area is thus drawn in one go. With the earlier strip of 20 lines the whole face drawing
    ran ~18× per frame, and that cost the mascot half of its drawing time.
  - The flush rotates the area in strips of 10 lines by 90° into one of two DMA buffers (internal,
    2 × 9.3 KB) and sends them asynchronously. The next strip is already being rotated while the
    previous one is still being sent.
  - LVGL keeps the AA edges of 16 circle radii (`LV_DRAW_SW_CIRCLE_CACHE_SIZE`).
  - Maximum 30 fps (`LV_DEF_REFR_PERIOD`).
- **Mascot at rest:** only eyes, eyebrows and mouth are redrawn. Hair and glasses stay still.
- **Draw nothing when nothing moves:** all springs snap to their target as soon as they settle.
  Breathing goes in whole pixels. If the face is covered by a panel, it is not redrawn.
- **Lower refresh rate with little movement:** sleepy ~10 fps, asleep ~8 fps.
- **Main loop:** waits until there is work (`delay` based on `lv_timer_handler`).
- **Touch:** only read on an interrupt, with a safety net every 100 ms.
- **Load:** awake and still ~35–45%, was ~95%. The CPU deliberately stays at 240 MHz.
- **Updates:** when an update starts, it wakes up first. From sleep mode, uploads would otherwise
  get stuck.
- **`perf` command:** fps, frame time (draw/rotate/send), main loop load and free internal RAM
  (including the lowest point). Measured (Wi-Fi connected, BLE on):

  | Look, emotion (continuously moving) | draw per frame | loop load |
  |---|---|---|
  | Robot, sleepy (10 fps) | 17 ms (was 34) | 48% (was 59%) |
  | Robot, in love | 15 ms (was 30), 23–25 fps | 77% |
  | Lisa, sleepy (10 fps) | 53 ms (was 79) | 80% (was 87%) |
  | Mascot, sleepy (10 fps) | 60 ms (was 84) | 86% (was 90%) |

  Still face: 0 fps, ~18% load. Internal RAM: ~80 KB free with Wi-Fi and BLE on (was
  ~52 KB).
- **Internal RAM is the scarce resource:** the BT controller (prebuilt) holds ~60 KB. Therefore:
  - the NimBLE host, the LVGL draw buffer and TLS (updates) are in PSRAM;
  - ordinary `malloc`/`new` goes to PSRAM from 256 bytes (`net::updateMemory`, by default only from
    4 KB). During a firmware update temporarily back to 4 KB: writing flash from
    PSRAM buffers made the upload ~5× slower (120 s instead of ~25 s);
  - the DMA buffers are small (10 lines).
- **Rebuilding ESP-IDF itself** (pioarduino `custom_sdkconfig`, e.g. to put Wi-Fi/lwIP buffers in
  PSRAM) does not work yet: the build fails on `SCons.Tool.FortranCommon` (platform 55.03.311).
- **During updates:** Matrix animation (only the eye windows redrawn) and a pump of at most ~10
  fps, so the download keeps priority.

## Appearance (construction kit)

The appearance is a **look** (`lib/buddy/look.*`): a base plus a style per part and a few colors.
The emotions still drive the eyelids, eyebrow angle, mouth etc., and each part draws that in its
own way (`face_robot.cpp`, `face_mascot.cpp`). Everything is drawn in code, so no images are
needed, and a look is only a few bytes in NVS.

| Part | Robot | Cartoon |
|---|---|---|
| Eyes | block, round, pill, ring + pupil, LED pixels, visor | normal (white + iris), dots |
| Eyebrows | none, bar, thick, thin, pixels | thick, straight, thin, none |
| Mouth | per emotion, line, zigzag, LED equalizer (moves with sound), pixels | per emotion (D-smile), closed smile |
| Cheeks | none, blush, stripes, pixels | blush, freckles, none |
| Glasses | none, square, round, sunglasses, monocle | square, round, sunglasses, monocle, none |
| Hair | | spikes, neat, curls, mohawk, bald, cap |
| Colors | eyes/mouth, eyebrows, background, glasses | iris, hair (or cap), skin, glasses |

- **Characters:** Robot, Mascot, Retro, Pixel, Cool, Synth, Professor, Rocker, Curly and
  Sporty. "Surprise me" makes a random combination with matching colors.
- **Emotion colors on/off:** when "on", angry, sad, in love and dizzy tint along; when "off", the
  robot always keeps its own color.
- **Choosing:**
  - in the web interface/PWA under **Appearance** (you see it directly on the buddy);
  - with a tap on the Appearance row in the info panel (next character);
  - with the `look` command.
- AMOLED tip: a black background (robot) is the most economical; every lit pixel costs power.

## Microphone (listening)

The two microphones (ES7210, on the same I2S bus as the speaker) are read in the audio task.
`lib/buddy/mic.*` turns the sound level into events. Nothing is recorded or sent: only the level
per 10 ms counts.

| Sound | Detection | Reaction |
|---|---|---|
| Clap | short, sharp peak (≤ 70 ms), clearly louder than everything in the last seconds | get startled, or wake up |
| Double clap | two claps within 0.12–0.7 s | happy little jump (the single clap is then not reported) |
| Talking | a large part of the time voice-like sound above the background, with pauses and without a steady beat | listening: an ear alternately left and right, waves come from outside toward the ear, the eyes follow along |
| Music | at least 8 regular beats (60–180 bpm) in 5 s, almost continuous sound | dancing: hop and alternately look to the side on the beat, the equalizer mouth moves along |
| Loud noise | ~1.5 s sustained far above the background, without beats | squeeze eyes shut (pain) |

- **Background** = the median of the sound (~3 dB/s adaptation). A fan or PC that hums continuously
  therefore does not count as voice.
- **Own sound:** as long as the buddy itself makes sound (and 0.3 s after), it does not listen.
- **On/off:** `mic on|off` or the button in the web app (Settings). Turning it off also ends
  listening and dancing.
- **Diagnostics:** `mic` (level, background, talking/music/dancing) and `mic trace` (last 1.5 s in
  dBFS).
- **Testing without noise:** the web emulator has buttons for clap, double clap, noise, music and
  talking. The regression test checks all reactions, and also that talking does not trigger an
  emotion.
- **Cost:** no extra internal RAM. While listening, the screen refreshes at ~15 fps, and only
  around the ear.

## Sound

Short robot sounds are computed live (`lib/buddy/sound.*`, 16 kHz). So no sound files are
needed. There is a sound for happy, in love, surprised, sad, angry, dizzy, yawning, waking up,
stroking (purring) and tapping. On the device the sound goes via the ES8311 codec and I2S to the
speaker on MX1.25. The amplifier (GPIO46) is only on during playback. You switch sound **on or off
in the info panel** (swipe down, tap "Sound"). Sound, volume, brightness and skin are saved in NVS.

> The ES8311 init (`src/board/audio.cpp`) is a custom, minimal register sequence. It has not been
> tested on the device yet.

## Language

The firmware and the web app exist in English and Dutch.

- Setting `lang`: `en` (default) or `nl`. Stored in NVS under the key `lang`.
- Command `lang en|nl` (serial, BLE and HTTP via `POST /api/cmd`). Without an argument it shows the
  current language.
- `/api/status` and the `json` command contain the field `"lang"` (`"en"` or `"nl"`).
- Switching the language changes the on-screen text (info panel, clock, setup screens) at once.
- In the web app and PWA there is an EN/NL switch. It sets the language of the device itself (the
  `lang` command), so the screen and the app always speak the same language.
- Serial and API replies, and the logs, are English-only and do not change with the setting.
- New UI text: add it in both languages (firmware `tr(en, nl)` in `lib/buddy/src/i18n.h`, web app
  dictionary in `firmware/web/index.html`).

## Buddies nearby (ESP-NOW, prototype)

Buddies in the same group see each other in the same room and catch each other's yawns. Design,
research and limits: [multi-buddy.md](multi-buddy.md). The web app has all of this in its Buddies tab.

**Groups**
- A group is a random 128-bit key (from the hardware random number generator) plus a name of 1–20
  characters. `group create <name>` makes one; the buddy is then its first member.
- **Invite:** `<key as 32 hex>.<name>`. `group invite` gives the invite and the link
  `https://newmedia.github.io/deskbuddy/?join=<invite>`; `group invite show` puts a QR with that link on
  the buddy's screen (tap closes it, otherwise 60 s). Scanning it with the phone that controls your own
  buddy opens the PWA, which shows "Invitation to join …" in the Buddies tab and joins over Bluetooth.
  `group join <invite or link>` does the same by command. Whoever has the invite has the key: share it
  only with people who may join.
- **Searching:** a visible group sends its name and a public id (the first 4 bytes of a hash of the
  key) every 5 s. `groups` listens for 30 s (ESP-NOW then also runs without a group) and lists the
  visible groups with their number of buddies. The name is only a label: joining still needs the
  invite. `group visible off` stops the name beacons.
- `group` shows name, id and visibility (never the key); `group off` leaves the group. The invite and
  the visibility are stored in NVS (`group`, `gvis`).

**Messages**
- Every 3 s a buddy sends a `hello`; a buddy not heard for 10 s drops off the list (max 8). `buddies`
  gives the list as JSON (own name, counters `tx`/`rx`/`bad`, and per buddy name, look, age and signal
  strength). `/api/status` and `json` have `"group"` and `"buddies"`; the info panel shows "2 buddies
  nearby" instead of the version.
- Messages are small JSON (~95 bytes) with an HMAC-SHA256 over the group key, truncated to 8 bytes,
  plus a boot id and sequence number against replays (`lib/buddy/src/mesh.cpp`, `sha256.cpp`). Name
  beacons are unsigned (everyone must be able to read them).
- **Contagious yawn:** a buddy that gets sleepy by itself sends a `yawn` event. A buddy that is awake
  yawns after 0.8–2 s (sleepy for 3.5 s), at most once per 30 s, and does not pass it on.

**Transport**
- Device: `src/net/espnow.cpp`, broadcast on the current Wi-Fi channel, only while there is a group
  (or a search) and Wi-Fi is on; received frames go via a queue (PSRAM) to the main loop.
- Web emulator: the same frames over UDP on localhost (ports 47801–47816), so several emulators on one
  computer see each other; each has its own name from its HTTP port.

## Serial commands (115200 baud)

The same command protocol is also used via WebSocket, BLE and MQTT later.

```
emotion <neutral|happy|sad|angry|surprised|sleepy|love|dizzy|asleep|cry|hyper|pain> [ms]
play <whistle|wink|kiss|rage|explode>
look <x> <y> | look off      # -1..1
blink | bounce
gesture <tap|double_tap|long_press|pet|swipe_left|...|shake|pick_up>
brightness <0-255>
sound on|off|<blip|happy|love|surprised|sad|angry|dizzy|yawn|wake|purr|toggle>
volume <0-100>
skin classic|mascot
lang [en|nl]                  # language; without argument = show current
group [create <name>|join <invite>|invite [show]|visible on|off|off]   # buddies nearby: group
groups                        # search 30 s for visible groups nearby
buddies                       # buddies nearby as JSON
wifi status | setup | on | off | forget | scan | join "<ssid>" <password>
ble on | off | forget | status
json                          # status as JSON (for BLE clients)
update [check|install|auto|notify|off]
clock | info | status | help
```

## Structure

| Path | Contents |
|---|---|
| `lib/buddy/` | Hardware-independent (LVGL only): `face` and `face_mascot` (procedural face, two skins), `face_fx` (actions and effects), `sound` (synth), `behaviour` (emotions and boredom), `gestures` (touch and IMU), `clock_view`, `info_view`, `commands`, `mesh` + `sha256` (buddies nearby), `app` (ties everything together and handles navigation) |
| `src/board/` | Hardware: CO5300 display via QSPI, CST9217 touch, QMI8658 IMU, AXP2101 PMU and PCF85063 RTC |
| `src/main.cpp` | Device: polling loop, serial commands, buttons |
| `src/sim/` | Headless PC simulator that runs the same `App` (PNGs, GIF and MP4 with sound) |
| `src/emu/` | Interactive emulator (SDL2 window with sound) |
| `include/lv_conf.h` | LVGL configuration (RGB565, fonts Montserrat 14/20/28/48) |

### Face
The eyes are rounded rectangles. Every emotion is a *pose*: eye size, corner radius, upper and
lower eyelid with tilt, color, blush, mouth and heart shape. All values run via springs to the
target, which is why emotions flow into each other. On top of that come blinking, looking around,
breathing, perspective when looking sideways and squash & stretch. A shape change (for example to
hearts) happens behind a blink. Only the area around the eyes is redrawn every frame.

Adding a new emotion: put a value in `Emotion` and a line in `Face::poseFor()` (`face.cpp`), and
add the name to `kNames`.

## Building, previewing and flashing

```bash
tools/setup.sh        # venv with esptool, pyserial and PlatformIO
tools/sim.sh          # simulator -> sim-out/emotions.png, mascot.png, demo.gif, demo.mp4 (with sound)
tools/sim.sh check    # one overview per character of all emotions and actions -> sim-out/check_NN.png
tools/emu.sh          # interactive emulator (once: sudo apt install libsdl2-dev)
tools/webemu.sh       # web emulator with web interface and PWA -> http://localhost:8765/emu
cd firmware && ../.venv/bin/pio run -e esp32   # build only
```

**XL (2.16″, square 480×480):** every env has an `-xl` twin and the scripts take `--xl`:
`tools/sim.sh --xl` (-> `sim-out-xl/`), `tools/emu.sh --xl`, `tools/webemu.sh --xl`
(-> http://localhost:8766/emu, own settings in `.pio/webemu-xl-state/`), `pio run -e esp32-xl`.
The screen size comes from `lib/buddy/src/screen.h`, the panel settings from `src/board/pins.h`;
see [xl-port.md](xl-port.md).

```bash
tools/flash.sh        # build + flash the Original (asks for confirmation, requires a backup of this chip)
tools/flash.sh --xl   # the same for the XL (both boards have the same chip: choose the right one)
tools/monitor.sh      # serial log and commands
```

Going back to the original firmware works with
`esptool --chip esp32s3 write-flash 0 backups/<file>.bin`. `tools/backup-flash.sh` puts the chip's MAC
in the file name (`backups/backup-<mac>-<date>.bin`); `tools/flash.sh` only flashes when there is a
backup of the connected chip.

## Web emulator (test everything without a device)

`tools/webemu.sh` runs the firmware logic on the PC, so the same `App`, the same face and the same
commands. It offers the same web API as the device. Open
**http://localhost:8765/emu** (other port: `tools/webemu.sh 9000` or `PORT=9000`). Only reachable
from this computer.

| Part | How |
|---|---|
| Screen | live in the browser; mouse = finger (tap, long press, stroke, swipe down/up) |
| Sensors | tilt (arrows), shake, pick up |
| Power | USB or battery (slider), charging |
| Wi-Fi | connected, setup network or off, signal strength. Wi-Fi setup works with fake networks; network **Gone** = not found, password **wrong** = wrong password |
| Web interface | on the right in a phone frame: `firmware/web/index.html` straight from disk, so edit and reload |
| PWA (Bluetooth) | "PWA" tab: the same page with `?pwa`, with a simulated Web Bluetooth (same NUS line protocol, replies in chunks of 20 bytes). Pairing works for real: the code appears on the screen. With `?pwa&auto` it goes without asking (for tests) |
| Updates | "offered release" determines what the simulated GitHub offers (empty = none). `update check/install` and uploading via the web interface show the install screen and restart |
| Sound | button "Sound in the browser on": the synth is streamed live |
| Restart | the emulator really restarts, with the saved settings |

Settings, custom profiles, the Bluetooth pairing and the "installed" version are preserved in
`firmware/.pio/webemu-state/` (like NVS). `tools/webemu.sh --reset` starts clean.

### Regression test

`tools/webemu-test.sh` (`-v` also shows the passed checks) tests both boards: per board it builds env
`webemu` or `webemu-xl` (only one: `--board original|xl`) and starts it on a
free port with its own, empty state (`WEBEMU_STATE`, your regular `webemu-state` stays untouched).
Then `tools/webemu_test.py` checks everything over HTTP: all emotions and actions (the lists come
from `/emu/state`, so new ones are included automatically), blink/bounce/shake, characters and
custom profiles, Wi-Fi scan and connect (wrong password, network gone, correct), BLE (pairing code,
wrong code, line commands, bonded), swiping to info and clock (`view` in `/emu/state`), sound (PCM
after `play whistle`), language, and updates (check, install, restart with new version and saved
settings, upload via `POST /update`), plus the board and screen size. Takes about 80 s per board. Ends with an overview per group; exit code 0
means all good, otherwise 1 plus the end of the emulator log. Run it before a release.
Guide (reading the result, adding checks, problems): [regression-test.md](regression-test.md).

Not simulated: real radios (Wi-Fi/BLE range), TLS to GitHub, flash writing, and the speed of the
real screen. So always measure performance on the device (`perf`).

## Emulator controls

With the mouse you operate the screen as if it were your finger: click = tap, hold = long press,
drag back and forth = stroke, swipe down or up = info or clock.

| Key | Action |
|---|---|
| 1–9 | emotion (neutral … asleep) |
| S / P | shake / pick up |
| arrows | tilt |
| B | blink |
| C / I | clock / info |
| W | cycle through Wi-Fi status (off → connecting → connected → AP) |
| L | BLE on/off |
| M | sound on/off |
| K | switch skin |
| F / N / J / R / E | whistle / wink / kiss / rage / explode |
| H / G / O / U | sneeze / laugh / eye roll / hiccup |
| + / - | battery |
| Esc | quit |

## Still to verify on the real device
- Sound: the ES8311 init, the volume and whether GPIO46 does switch the amplifier
- Whether the tilt axes are right (`Behaviour::tiltSignX/Y`) and whether the thresholds for shaking
  and picking up are good
- Frame rate. If needed raise the QSPI clock (`gfx->begin(80000000)`) or put the buffers in DMA RAM.
- Whether touch and tilt work in the same direction as the image. The screen is permanently rotated
  by 90° (`kRotation` in `src/board/board.cpp`), because the board sits rotated in the case. Touch
  and IMU rotate along.
