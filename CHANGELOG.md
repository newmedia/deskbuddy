# Changelog

Each version below becomes a release on GitHub (see `tools/release.sh` and `docs/firmware.md`).
The text under a heading is the description of that release. Newest first.

## v1.4.0-beta.1

Test version: buddies in the same room see each other and catch each other's yawns.

- **Buddies nearby (prototype)**: buddies in the same group see each other over ESP-NOW, without a
  router in between, and the info panel shows "2 buddies nearby". Off by default.
- **Groups**: create a group with a name in the new Buddies tab. Others join with the invite: show it
  as a QR on your buddy and scan it with the phone that controls theirs, or share the link. Visible
  groups can be found by searching (only the name; joining always needs the invite). Messages are
  signed with the group's random key, so buddies outside your group cannot do anything with yours.
- **Contagious yawn**: when a buddy gets sleepy by itself, the others nearby yawn along (at most once
  per 30 seconds, and without yawning back and forth).
- **Web app**: the tabs are icons now (control, buddies, settings, Wi-Fi, system), with the name as
  a tooltip; buddies have their own tab.
- **Test versions**: buddies never update to a test version by themselves, but a buddy on a test
  version does move on to the next regular version. Install it via the install page with
  "Show test versions".
- For now all buddies must be on the same Wi-Fi network (same channel). Not yet measured on real
  buddies; feedback welcome.

## v1.3.0

The XL (2.16″, square screen) gets its own firmware, and a black screen on new buddies is fixed.

- **Fix: black screen without a saved appearance**: a buddy that never saved its own appearance got
  an empty one with black eyes on a black screen (effects such as the explosion were still visible).
  This was very probably also the "black circle" after v1.1.1. Buddies that are affected are fixed
  by this update on their own.
- **XL (Waveshare ESP32-S3-Touch-AMOLED-2.16)**: its own firmware (`firmware-xl.bin`) with the
  square 480×480 screen, hardware rotation, touch and motion sensor set up for this board. Each
  buddy only installs the firmware for its own board.
- **Install page**: first choose the board (Original or XL), then the version.
- **Web app**: the board is shown under System → Status.

## v1.2.0

English and Dutch: the buddy and the web app speak both, and the whole project is now in English.

- **Language**: choose English or Nederlands in the web app or PWA (Settings, or EN/NL in the footer).
  The choice is stored on the buddy and applies to everything on its screen: info panel, clock,
  Wi-Fi setup, pairing code and update screen. English is the default; serial command `lang en|nl`.
- **English project**: docs, code comments, scripts and the regression test are in English. Serial
  and API replies are English only (e.g. Wi-Fi reasons "wrong password", "network not found").
- **Characters**: Mascotte, Krullebol and Sportief are now called Mascot, Curly and Sporty. A custom
  look is named "Custom" (or "Eigen" in Dutch), a random one "Surprise".
- **Fix: black circle after the boot screen**: the logo now really fades out and the screen is fully
  cleaned up afterwards. Before, the (black) logo circle sometimes stayed over the eyes on the device; big effects such as
  the explosion were still visible around it.
- **Name**: the buddy is now called **deskbuddy.dev**, on the boot screen, in the info panel, in the
  web app and on the install page. The footer has a link to https://deskbuddy.dev.
- **Web emulator**: `reboot` now always restarts (the restart sometimes slipped forever), and after a
  restart the process is still called `program`, so `pkill -x program` keeps finding it.
- **tools/setup.sh** picks a Python that supports PlatformIO with the ESP32 platform (3.10-3.14) and
  recreates the venv if it was made with a different version.

## v1.1.1

The deskbuddy logo is now everywhere: on the buddy's screen, in the web app and on the install page.

- **Boot screen**: when powered on, the buddy briefly shows the logo with its name and version, and
  then fades smoothly into the face.
- **Info panel**: a small logo next to the version line.
- **Web page on the device** (and the PWA): the same logo in the header and as the tab icon, instead
  of the earlier custom version without a smile.
- **Install firmware over USB**: new section in the System tab and a footer with links to
  the install page (https://newmedia.github.io/deskbuddy/flash/), where you can also pick an older
  version.

## v1.1.0

The buddy now listens along: it reacts to claps, talking, music and noise.

- **Microphone**: the two built-in microphones are now used. Only the sound level is measured;
  nothing is recorded or sent.
  - **Clap**: it gets startled, or wakes up if it was sleeping.
  - **Double clap**: a happy little jump.
  - **Talking**: it listens. An ear sticks out alternately on the left and right, sound waves
    come in from outside toward the ear, and the eyes follow along.
  - **Music**: it dances to the beat, with hops and alternately looking to the side.
  - **Loud noise**: it squeezes its eyes shut.
  - It ignores ordinary background noise (fan, PC) and its own sounds.
  - On/off via the "Microphone" button in the web app (Settings) or the command `mic on|off`.
- **Manual updating**: the web app now points to the releases on GitHub to download
  `firmware.bin`.
- For developers: a web emulator (`tools/webemu.sh`) and an automated regression test
  (`tools/webemu-test.sh`) to test everything, including the web app and the PWA, without a device.

## v1.0.0

The first full version: its own look, new emotions, a revamped web app and a faster, more
economical buddy. From now on we follow semver: new features raise the middle number
(1.1.0), bugfixes only the last (1.0.1).

- **Look as a construction kit**: choose robot or cartoon and compose eyes, eyebrows, mouth, cheeks,
  glasses, hair, lashes, lipstick, earrings and hair accessories yourself, with your own colors.
  There are 16 ready-made characters (including Lisa, Noor, Sofie, Mila, Emma and Bo) and a
  "surprise me". You save your own looks under a name (up to 8 profiles).
- **New emotions**: crying (tears down the cheeks), busy/ADHD (darting gaze and
  little jumps) and pain (grimace, sweat drop and an occasional sting).
- **Bouncing and shaking really visible**: bouncing is a little jump, the hiccup a jolt upward, and
  during rage, exploding and pain the whole face shakes. Before, you saw none of that.
- **Revamped web app**: tabs (controls, settings, Wi-Fi, system) and an editor for
  the look that you toggle with the palette icon. The editor has buttons instead of
  drop-down lists and color swatches for each part. At the top you see how it is connected (Wi-Fi with
  signal strength, Bluetooth or setup) and whether it runs on USB power or on the battery.
- **Faster and more memory**: the screen is drawn in one go instead of in 18 strips,
  making drawing 30-50% faster. There is ~80 KB of free working memory with Wi-Fi and Bluetooth on
  (was ~52 KB), as room for new features.
- **Bugfixes**:
  - wink and kiss: the closed eye was invisible on the cartoon;
  - tears and sleep Z's disappeared behind long or dark hair;
  - after waking up a Z sometimes stayed behind;
  - with the visor a small block stayed behind during sleep.

## v0.3.2

More economical: the buddy only draws when something moves, and updates also work from sleep mode.

- **Much lower CPU usage** (from ~95% to ~40% when awake and standing still): nothing is
  drawn when nothing moves, breathing steps by whole pixels, and the face is not
  redrawn when the clock or the info panel covers it.
- **Sleepy and sleeping at low frame rate** (~10 and ~8 frames per second), which saves the battery.
- **Updates from sleep mode**: when an update starts, the buddy first wakes up.
  Before, uploads from sleep got stuck.
- **Touch** is only read when the screen is touched.
- New command `reboot` (restart remotely).

## v0.3.1

Updates and Bluetooth now work together, plus new actions and faster display control.

- **Updates with Bluetooth on**: during an update BLE only stops advertising instead of
  turning off completely. In v0.3.0 the buddy could lose its Wi-Fi connection because of that; with
  v0.3.0, first turn off BLE or updates, or install this version manually.
- **More free working memory with Bluetooth on** (from a few KB to well over 50 KB), so that the web
  interface and updates stay stable.
- **Own display driver**: faster drawing via asynchronous DMA; the mascot at rest only
  redraws what moves.
- **New actions**: sneezing, laughing (3x fast stroking = tickling), rolling eyes (2x fast swiping
  back and forth) and the hiccup, with sound and buttons in the web interface.

## v0.3.0

Bluetooth, an app for your phone and a faster, more economical buddy.

- **Bluetooth (BLE)**: control without Wi-Fi via the Nordic UART Service, with the same
  command protocol. Pairing with a PIN on the screen; only paired devices get
  access. On/off via the info panel or `ble on|off|forget`.
- **Web app (PWA)**: the web interface can be installed on your phone and works over Wi-Fi or via
  Web Bluetooth.
- **Updates alongside Bluetooth**: while checking and installing, BLE briefly turns off, so there is
  enough memory for the secure connection to GitHub. An automatic check waits
  if a phone is connected.
- **Matrix animation during updates**: green eyes with falling code and a progress ring, also
  when uploading via the web page and ArduinoOTA.
- **Faster and more economical**: a lot more free working memory, firmware 160 KB smaller, a sharper clock,
  faster display control and a smoother web interface (no Wi-Fi sleep mode).
- New commands: `reboot`, `perf`, `json` and `wifi scan`; `wifi join` accepts network names
  with spaces in quotes.

## v0.2.0

First release that the deskbuddy can fetch by itself.

- **Automatic updates**: at startup and every 12 hours the deskbuddy checks whether there is a newer
  release on GitHub. By default it reports that (info panel and web UI, with a button to
  install); with `update auto` it installs by itself. While installing it looks surprised and
  a progress ring runs around the screen.
- **Actions with sound**: whistling (spontaneously when bored), wink (double tap), kiss (5x stroking)
  and with repeated tapping angry, then rage and finally exploding. Also via `play` and the web UI.
- **Wi-Fi setup in two steps**: QR code for the setup network, then a QR code with the
  setup link; captive portal on Android, iOS and Windows, and feedback while connecting.
- **Web interface** at `http://deskbuddy.local`: emotions, look, sound, volume, brightness,
  sleep time, Wi-Fi and firmware updating.
- **Adjustable sleep time** (1-120 minutes or never).
- Time via NTP, with the RTC as backup.

## v0.1.0

First own firmware (not published as a release): procedural face with emotions, touch and
motion gestures, clock, info panel, sound effects and two skins (robot and mascot).
