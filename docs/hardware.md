# Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75

The deskbuddy runs on a standard Waveshare development board in a 3D-printed case. At the moment
it still runs the original closed-source firmware (see `docs/original-firmware.md`).

- Board: https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm (± $30–40)
- Wiki: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75
- Example code: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75

## What the laptop sees (measured 2026-10-15)

| Field | Value |
|---|---|
| USB ID | `303a:1001` — Espressif *USB JTAG/serial debug unit* |
| Serial number | `44:BD:8D:79:A4:5C` (= MAC address of the chip) |
| Interfaces | `02/02/00` CDC-ACM + `0a` CDC-data + `ff/ff/01` JTAG |
| Linux device | `/dev/ttyACM0` → `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_44:BD:8D:79:A4:5C-if00` |
| Permissions | `root:dialout` — user is **not** in `dialout` |

`303a:1001` is the ESP32-S3's built-in USB-Serial/JTAG controller itself (no separate USB-UART
chip). That means you can flash, read logs and even debug over JTAG through the same USB-C cable.
The firmware does not set up any USB function of its own (no HID or mass storage), so the USB port
is only used for power, logs and flashing.

## Specifications

| Component | Chip | Bus |
|---|---|---|
| SoC | ESP32-S3R8, dual-core Xtensa LX7 @ 240 MHz, 512 KB SRAM | — |
| Memory | 8 MB PSRAM (in-package), 16 MB external flash | — |
| Radio | 2.4 GHz Wi-Fi b/g/n, Bluetooth 5 LE, onboard antenna (IPEX optional) | — |
| Display | 1.75" round AMOLED, 466×466, 16.7M colors, 700 cd/m² — driver **CO5300** | QSPI |
| Touch | **CST9217** capacitive | I2C |
| IMU | **QMI8658** 6-axis (accelerometer + gyroscope) | I2C |
| RTC | **PCF85063** (keeps running on the battery) | I2C |
| PMU | **AXP2101** (charging, voltages, battery measurement) | I2C |
| IO expander | **TCA9554** | I2C |
| Audio out | **ES8311** codec + amplifier, MX1.25 speaker connector | I2S |
| Audio in | **ES7210** echo-cancelling ADC + 2 microphones | I2S |
| Storage | microSD slot | SDMMC |
| Other | PWR and BOOT button, MX1.25 3.7 V Li-ion connector, 8-pin 2.54 mm header (3× GPIO + UART) | — |

There is also a `-G` variant with an LC76G GPS module.

## Pin mapping

Source: `examples/arduino/libraries/Mylibrary/pin_config.h` in Waveshare's repo.

| Function | GPIO |
|---|---|
| LCD QSPI D0–D3 | 4, 5, 6, 7 |
| LCD SCLK / CS / RESET | 38 / 12 / 39 |
| I2C SDA / SCL (touch, IMU, RTC, PMU, expander) | 15 / 14 |
| Touch INT / RESET | 11 / 40 |
| I2S MCLK / BCLK / WS | 42 (16 in the ES8311 example) / 9 / 45 |
| I2S data | 8, 10 |
| Speaker amplifier enable (PA) | 46 |
| SD CLK / CMD / D0 / CS | 2 / 1 / 3 / 41 |

## Download or boot mode

Hold **BOOT**, briefly press **PWR**/reset (or connect USB) and release BOOT. Through the
USB-Serial/JTAG, esptool can usually reset the chip without any buttons as well.
