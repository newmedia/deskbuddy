# Original firmware

## What it is
- Closed-source firmware, supplied only as a **ready-to-flash binary** with the digital download.
  The source code is deliberately not included.
- Supplied with 3D-print files for the case (the figures, accessories) and an assembly guide.
  The case is modular: heads, bodies and accessories are interchangeable.

## Features (according to the product page)
- Animated face with emotions that reacts to touch
- Clock, timer with alarm and battery indicator
- Works **fully offline**
- Optional battery: the board has a Li-ion connector and the AXP2101 handles charging

## What else the hardware can do
The backup shows that the firmware uses the display, touch, PMU, IMU and speaker (see below).
- **Wi-Fi/BLE** (confirmed unused): for example for NTP time, notifications, Home Assistant or a link to the PC
- **Microphones** (the speaker *is* used): speech, sounds or wake word (Waveshare demonstrates AI chat)
- **microSD**: custom animations or assets

So this board is a good basis for custom firmware in the `deskbuddy` project.

## Important before you flash anything yourself
The original firmware exists only on the chip. **First make a full flash backup** with
`tools/backup-flash.sh`. Without a backup you cannot go back, except by downloading it again from
the original maker. The backup is for your own use only. Do not distribute it, because the firmware
falls under the original maker's IP.

## Analysis of the flash backup (2026-10-15)

Backup: `backups/original-firmware-20261015-205611.bin` (16 MB, sha256 `54b3dc52…bf4c0a`, checked with
`verify-flash`). The backup is not in git.

### Chip
ESP32-S3 (QFN56) rev v0.2, 8 MB embedded PSRAM, 16 MB quad flash (manufacturer 0x20, device 0x4018),
40 MHz crystal, MAC `44:bd:8d:79:a4:5c`. There are no signs of secure boot or flash
encryption: the image is readable and the boot log reports nothing.

### Partition table (Arduino default `default_16MB.csv`)
| Name | Offset | Size | Contents |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | namespace `settings`, key `volume` (u8) |
| otadata | 0xe000 | 8 KB | active slot = **app0** |
| app0 | 0x10000 | 6400 KB | original firmware (~2.9 MB used) |
| app1 | 0x650000 | 6400 KB | empty (OTA slot, unused) |
| spiffs | 0xc90000 | 3456 KB | empty (all 0xFF) |
| coredump | 0xff0000 | 64 KB | — |

All assets (images, fonts, animations) are therefore **in the app binary itself** and not in a
file system.

### Framework and libraries
- **PlatformIO**, board `esp32-s3-devkitc-1`, **Arduino-ESP32 2.0.x** on ESP-IDF v4.4.7, built
  on Windows
- **LVGL 8.x**, with the widgets arc, roller, dropdown, menu, chart, textarea, img and png. The
  `src/ui/` folder with `ui_theme_manager.c` and `_ui_local_style_*` points to a UI made with
  **SquareLine Studio**.
- **GFX Library for Arduino** (`Arduino_ESP32QSPI`) for the CO5300 display
- **SensorLib** (lewisxhe) for touch and IMU. At startup the firmware tries CST226, CST816 and
  CST92xx in turn; the last one is the real chip, the CST9217.
- **XPowersLib** for the AXP2101 (battery and charging)
- **ES8311 + I2S**: the speaker is actually used (volume setting in NVS)
- **QMI8658** IMU: is initialized ("QMI8658 initialized!")
- `Preferences` (NVS) and `Wire` (I2C)

### Not used
- **No Wi-Fi and no BLE**: there are no traces of `esp_wifi_init`, NimBLE/Bluedroid or SNTP.
  The firmware really is offline. The HTTP error names in the binary come from the standard
  `esp_err` table.
- No SPIFFS/LittleFS and no SD card

### Boot log (115200 baud over USB-CDC)
The log shows a reset via USB, loads app0 and then prints a few harmless error messages
(`Wire setPins`, touch chip ID mismatch) while searching for the right touch driver. It ends
with `QMI8658 initialized!`.

## Open questions
- What is the IMU used for: taps, shaking or tilting? This can be found out by
  testing with it.
- Which sounds does the speaker play?
