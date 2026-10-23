# Porting to the XL (Waveshare ESP32-S3-Touch-AMOLED-2.16)

deskbuddy.dev comes in three sizes: Mini (1.43″), Original (1.75″) and XL (2.16″). The firmware runs
on the Original. This document compares the XL board with it and plans the port. Status
(2026-10-23): phases 1–3 done (separate XL build, screen size in the lib, XL display driver); the XL
firmware builds and runs in the sim and web emulator, but has **not been flashed** on the XL yet.

## The board on the bench

Measured read-only on 2026-10-23 (`tools/chip-info.sh` and the boot log):

| | Value |
|---|---|
| Board | Waveshare ESP32-S3-Touch-AMOLED-2.16 (the boot log tags itself `ESP32-S3-Touch-AMOLED-2.16`) |
| Chip | ESP32-S3 (QFN56) rev v0.2, 8 MB embedded PSRAM (AP, octal), 40 MHz crystal |
| Flash | 16 MB quad, manufacturer 0x20, device 0x4018 (same as the Original) |
| MAC | `28:84:85:90:78:40` |
| Firmware on it | the user's own ESP-IDF v5.5.5 project "soundbox" (microSD sound library, `soundbox.local`) |
| Backup | `backups/soundbox-amoled-2.16-288485907840-20261023-111336.bin` (16 MB, checked with `verify-flash`) |

Partitions of the soundbox firmware: nvs, otadata, phy_init, ota_0 and ota_1 (4 MB each), storage
(8 MB). The deskbuddy firmware brings its own partition table, so flashing it replaces all of this;
restoring is a `write-flash 0` of the backup.

## Pinout: XL vs Original

Sources: Waveshare's `examples/arduino/libraries/Mylibrary/pin_config.h` in
[ESP32-S3-Touch-AMOLED-2.16](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.16), the ESP-IDF
BSP `bsp/esp32_s3_touch_amoled_2_16` in
[Waveshare-ESP32-components](https://github.com/waveshareteam/Waveshare-ESP32-components), and the
[wiki](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-2.16). All three agree.

| Function | Original (1.75″) | XL (2.16″) |
|---|---|---|
| LCD QSPI D0–D3 | 4, 5, 6, 7 | 4, 5, 6, 7 |
| LCD SCLK / CS / RESET | 38 / 12 / 39 | 38 / 12 / 39 |
| I2C SDA / SCL | 15 / 14 | 15 / 14 |
| Touch INT / RESET | 11 / 40 | 11 / 40 |
| I2S MCLK / BCLK / WS | 42 / 9 / 45 | 42 / 9 / 45 |
| I2S data out (ES8311) / in (ES7210) | 8 / 10 | 8 / 10 |
| Speaker amplifier (PA) | 46 | 46 |
| SD CLK / CMD / D0 / CS | 2 / 1 / 3 / 41 | 2 / 1 / 3 / 41 |
| BOOT button | 0 | 0 |
| Extra user button | — | **KEY3 on GPIO18** (pull-up, low when pressed) |
| RTC interrupt | — | GPIO13 |
| IMU interrupts | — | GPIO17 (INT1), GPIO21 (INT2) |
| SYS_OUT (PMU) | — | GPIO16 |

So every pin the firmware uses today is the same. The differences are in the parts:

| Part | Original | XL | Effect on the firmware |
|---|---|---|---|
| Display | 1.75″ **round**, 466×466, CO5300 | 2.16″ **square with rounded corners**, 480×480, CO5300 | Screen size and shape, see below |
| Column offset | 6 | **0** | `kColOffset` per board |
| Init sequence | our own (from Arduino_GFX) | BSP adds `FE 20`, `19 10`, `1C A0`, `FE 00`, TE on (`35 00`), window 0..479 | Use the BSP sequence on the XL |
| Rotation | in software (`lv_draw_sw_rotate`, ROTATION_270) | panel supports **hardware rotation** via MADCTL (`0x00`, `0x60`, `0xC0`, `0xA0`); the BSP default is `0xA0` | Saves the rotate step per frame (its cost on the Original shows as "rotate" in `perf`) |
| Touch | CST9217 | **CST9220** (boot log: chip type 0x9220), same `TouchDrvCST92xx` driver, address 0x5A | Only the coordinate mapping (BSP: swap_xy, mirror_y) |
| IMU | QMI8658 at 0x6B | QMI8658 at 0x6B | Axis mapping depends on how the board sits in the case |
| PMU | AXP2101 | AXP2101 | none |
| RTC | PCF85063 | PCF85063ATL | none |
| Audio | ES8311 + ES7210 | ES8311 + ES7210 | none |
| IO expander | TCA9554 | not listed | not used by the firmware anyway |

## What in the code assumes the Original

- `Face::kSize = 466` (face.h): face, effects and partial redraws all hang off this and its centre
  (`C`, `kCenter`). Changing it moves everything to the new centre.
- Hard-coded 233 (half of 466): `gestures.cpp` (`kHalf`, touch -> -1..1) and `splash_view.cpp`
  (logo position).
- View layouts with fixed numbers that were chosen for a round 466 screen: info panel widths
  (330/270) and bottom offsets, clock battery ring (446×446 arc), setup and update views, splash
  text positions. On a square screen the corners are visible, so these can use more room.
- `firmware/src/board/`: `pins.h` (466), `display.cpp` (column offset 6, init sequence, software
  rotation, DMA strip size), `board.cpp` (touch mapping "inverse of ROTATION_270", IMU axes).
- Emulators: `webemu` (`emu.html` canvas 466×466 with a round mask), `sim`, `emu`.
- Distribution: one `firmware.bin` per release; the updater, install page and web app know only one
  board.

Full-screen backgrounds are already rectangles (`fillRect(0, 0, kSize, kSize)`), so a square screen
does not show black corners there. Things that were hidden behind the round edge (particles,
mascot ears near the corners) will now be visible; check them in the sim.

## Plan

### Decision: one build per board (chosen 2026-10-23)

Two options:

1. **A separate build per board** (`esp32` for the Original, `esp32-xl` for the XL), chosen with a
   build flag. Screen size stays a compile-time constant, so the face code and its performance stay
   as they are. The updater downloads the asset for its own board (`firmware.bin` or
   `firmware-xl.bin`), and the install page asks which board you have.
2. **One binary that detects the board** at startup (e.g. CST9217 vs CST9220, or the touch
   resolution). Simpler distribution, but `kSize` becomes a runtime value throughout the face code,
   and a wrong detection means a black screen.

Option 1 is less work and less risk; the board is fixed anyway, unlike the language.

### Phases

1. **Board selection (no device needed).** Build flag `DESKBUDDY_BOARD_XL`; `pins.h` becomes a
   per-board config (size, column offset, init sequence, rotation, touch/IMU mapping, KEY3). New
   PlatformIO env `esp32-xl` that extends `esp32`. The Original build must stay byte-for-byte the
   same in behaviour.
2. **Screen size in the lib (no device needed).** One constant for the screen size (from the build
   flag, default 466); replace the hard-coded 233s; make view layouts relative to the screen size and
   use the corners on a square screen. Sim, emu and webemu get XL variants (`sim-xl`, `webemu-xl`,
   square canvas with rounded corners), and the regression test runs on both. Check all screens with
   `tools/sim.sh` for both sizes.
3. **XL display driver (no device needed to build).** BSP init sequence, window 0..479, column
   offset 0, hardware rotation via MADCTL instead of `lv_draw_sw_rotate` (with the software path as
   a fallback), DMA strips for 480 wide.
4. **Bring-up on the XL (needs your OK to flash; backup is in place).** Flash `esp32-xl`, then check
   in this order: picture and orientation, touch mapping, IMU axes (`tools/imu-measure.py`), audio
   (speaker and microphones), battery/PMU, RTC, KEY3, Wi-Fi/BLE, then `perf` (fps, frame time,
   free internal RAM).
5. **Distribution.** `tools/release.sh` builds both and uploads `firmware.bin` and
   `firmware-xl.bin`; the updater picks its own asset (a device never installs the other board's
   firmware); install page with a board choice and a manifest per board and version; the web app
   and `json` show the board; docs, changelog and the deskbuddy.dev "XL waits for firmware" texts.

### Done in phases 1–3 (2026-10-23)

- `firmware/lib/buddy/src/screen.h`: `kScreen` (466 / 480), `kSquare`, `kBoardName`, plus the
  visible-area mask for the emulators (`screenEdgeDistance`, `onScreen`). `Face::kSize = kScreen`;
  the hard-coded 233s (touch centre, splash logo) and the clock ring now follow the screen size.
- PlatformIO envs `esp32-xl`, `sim-xl`, `emu-xl` and `webemu-xl`: the Original env plus
  `-D DESKBUDDY_BOARD_XL`. The Original build is unchanged apart from a `"board"` field in the status.
- `"board"` (`original` / `xl`) in `/api/status` and `json`; `"board"` and `"screen"` in `/emu/state`.
- Scripts: `tools/sim.sh --xl` (-> `sim-out-xl/`), `tools/emu.sh --xl`, `tools/webemu.sh --xl` (port
  8766, own state), `tools/webemu-test.sh` tests both boards (`--board original|xl` for one). The web
  emulator page takes size and corner radius from the build.
- `src/board/pins.h` per board: `LCD_WIDTH/HEIGHT`, `LCD_COL_OFFSET`, `LCD_MADCTL`, `LCD_SW_ROTATION`,
  `KEY3_BUTTON`. `display.cpp` on the XL: BSP init additions (`FE 20`, `19 10`, `1C A0`), MADCTL
  `0xA0`, column offset 0, and a plain copy into the DMA buffer instead of the software rotation.
  Touch on the XL: `setSwapXY(true)` + `setMirrorXY(true, false)` like Waveshare's example for 0xA0.
- Updater: the XL looks for `firmware-xl.bin` in a release, so it can never install the Original's
  `firmware.bin`. Until releases contain that asset (phase 5), an XL reports "release has no
  firmware-xl.bin" when it checks for updates.

### Prepared for phase 5 (2026-10-23, not released yet)

- `tools/release.sh` builds `esp32` and `esp32-xl` and uploads `firmware.bin` and `firmware-xl.bin`
  (each with a `.sha256`).
- `tools/publish-flash.sh` also fetches `firmware-xl.bin` when a release has it, with its own
  `manifest-xl.json`; `versions.json` lists the boards per version. The install page asks for the
  board first and only offers XL versions that exist (none yet: it says so and hides the button).
- Web app: "Board" under System → Status; the install links open with `?board=xl` on an XL.
- `tools/flash.sh --xl` flashes the XL build. Because both boards have the same chip, it names the
  board in the confirmation and requires a backup of the connected chip (MAC in the file name;
  `tools/backup-flash.sh` now names its dumps `backup-<mac>-<date>.bin`).

Release the XL firmware only after phase 4: the first release with `firmware-xl.bin` makes it
installable from the install page. Until then use `tools/release.sh <version> --original-only`.

### To verify on the device (phase 4)

- Picture: orientation (MADCTL `0xA0`), no offset or tearing, colours; `perf` (the "rotate" time is now
  only the copy).
- Touch mapping and IMU axes (`tools/imu-measure.py`); both are guesses until measured.
- Corner radius of the visible area (`kCornerRadius` in `screen.h`, estimated at 40 px; only used by
  the emulators).
- Audio, battery/PMU, RTC, Wi-Fi/BLE: same chips and pins, so expected to work as on the Original.

### Open questions

- **Orientation in the XL case:** which side is up (where the USB port ends up) decides the MADCTL
  value and the touch and IMU mapping. The case is still a concept; until then use the BSP default
  (`0xA0`) and make it one setting.
- **KEY3:** what should it do? Suggestion: open the info panel (like PWR), or cycle emotions like
  BOOT does now.
- **Face size:** keep the face the same size in pixels (480 is 3% bigger), or scale it up a little
  to fill the larger, square screen.
