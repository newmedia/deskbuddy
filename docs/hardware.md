# Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75

De "Deskimon" (Orbit & Sprout) van Creative Chance is **geen eigen hardware**: het is een
3D-geprinte behuizing rond een standaard Waveshare-ontwikkelbord, plus closed-source firmware.

- Product (digitale download, €7,27): https://creative-chance.com/en-eur/products/orbit-sprout-deskimon
- Bord: https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm (± $30–40)
- Wiki: https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.75
- Voorbeeldcode: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75

## Wat de laptop ziet (gemeten 2026-10-15)

| Veld | Waarde |
|---|---|
| USB ID | `303a:1001` — Espressif *USB JTAG/serial debug unit* |
| Serienummer | `44:BD:8D:79:A4:5C` (= MAC-adres van de chip) |
| Interfaces | `02/02/00` CDC-ACM + `0a` CDC-data + `ff/ff/01` JTAG |
| Linux-device | `/dev/ttyACM0` → `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_44:BD:8D:79:A4:5C-if00` |
| Rechten | `root:dialout` — gebruiker zit **niet** in `dialout` |

`303a:1001` is de ingebouwde USB-Serial/JTAG-controller van de ESP32-S3 zelf (geen aparte
USB-UART-chip). Daardoor kun je flashen, logs lezen en zelfs JTAG-debuggen via dezelfde USB-C-kabel.
De firmware zet geen eigen USB-functie op (geen HID of mass storage), dus de USB-poort dient alleen
voor stroom, logs en flashen.

## Specificaties

| Onderdeel | Chip | Bus |
|---|---|---|
| SoC | ESP32-S3R8, dual-core Xtensa LX7 @ 240 MHz, 512 KB SRAM | — |
| Geheugen | 8 MB PSRAM (in-package), 16 MB externe flash | — |
| Radio | 2.4 GHz Wi-Fi b/g/n, Bluetooth 5 LE, onboard antenne (IPEX optioneel) | — |
| Display | 1.75" rond AMOLED, 466×466, 16.7M kleuren, 700 cd/m² — driver **CO5300** | QSPI |
| Touch | **CST9217** capacitief | I2C |
| IMU | **QMI8658** 6-assig (accelerometer + gyroscoop) | I2C |
| RTC | **PCF85063** (blijft lopen op de accu) | I2C |
| PMU | **AXP2101** (laden, voltages, accu-meting) | I2C |
| IO-expander | **TCA9554** | I2C |
| Audio uit | **ES8311** codec + versterker, MX1.25 speakeraansluiting | I2S |
| Audio in | **ES7210** echo-cancelling ADC + 2 microfoons | I2S |
| Opslag | microSD-slot | SDMMC |
| Overig | PWR- en BOOT-knop, MX1.25 3.7 V Li-ion-aansluiting, 8-pins 2.54 mm header (3× GPIO + UART) | — |

Er bestaat ook een `-G`-variant met een LC76G GPS-module.

## Pin-mapping

Bron: `examples/arduino/libraries/Mylibrary/pin_config.h` in Waveshare's repo.

| Functie | GPIO |
|---|---|
| LCD QSPI D0–D3 | 4, 5, 6, 7 |
| LCD SCLK / CS / RESET | 38 / 12 / 39 |
| I2C SDA / SCL (touch, IMU, RTC, PMU, expander) | 15 / 14 |
| Touch INT / RESET | 11 / 40 |
| I2S MCLK / BCLK / WS | 42 (16 in ES8311-voorbeeld) / 9 / 45 |
| I2S data | 8, 10 |
| Speaker-versterker enable (PA) | 46 |
| SD CLK / CMD / D0 / CS | 2 / 1 / 3 / 41 |

## Download- of bootmodus

Houd **BOOT** ingedrukt, druk kort op **PWR**/reset (of sluit USB aan) en laat BOOT los. Via de
USB-Serial/JTAG kan esptool de chip meestal ook zonder knoppen resetten.
