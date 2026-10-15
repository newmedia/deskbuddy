# Originele firmware

## Wat het is
- Closed-source firmware, alleen als **kant-en-klaar binary** ("ready to flash") geleverd bij de
  digitale download. De broncode zit er bewust niet bij.
- Geleverd met 3D-printbestanden voor de behuizing (de figuren, accessoires) en een bouwhandleiding.
  De behuizing is modulair: hoofden, lichamen en accessoires zijn onderling uitwisselbaar.

## Functies (volgens de productpagina)
- Geanimeerd gezicht met emoties dat reageert op aanraking
- Klok, timer met alarm en accu-indicator
- Werkt **volledig offline**
- Optionele accu: het bord heeft een Li-ion-aansluiting en de AXP2101 regelt het laden

## Wat de hardware nog meer kan
Uit de backup blijkt dat de firmware het display, de touch, de PMU, de IMU en de speaker gebruikt (zie hieronder).
- **Wi-Fi/BLE** (bevestigd ongebruikt): bijvoorbeeld voor NTP-tijd, notificaties, Home Assistant of een koppeling met de PC
- **Microfoons** (speaker wordt wél gebruikt): spraak, geluiden of wake-word (Waveshare demonstreert AI-chat)
- **microSD**: eigen animaties of assets

Dit bord is dus een goede basis voor eigen firmware in het `deskbuddy`-project.

## Belangrijk vóór je zelf iets flasht
De originele firmware staat alleen op de chip. **Maak eerst een volledige flash-backup** met
`tools/backup-flash.sh`. Zonder backup kun je niet terug, behalve met een nieuwe download van
de oorspronkelijke maker. De backup is alleen voor eigen gebruik. Verspreid hem niet, want de firmware valt
onder het IP van de oorspronkelijke maker.

## Analyse van de flash-backup (2026-10-15)

Backup: `backups/original-firmware-20261015-205611.bin` (16 MB, sha256 `54b3dc52…bf4c0a`, gecontroleerd met
`verify-flash`). De backup staat niet in git.

### Chip
ESP32-S3 (QFN56) rev v0.2, 8 MB embedded PSRAM, 16 MB quad-flash (fabrikant 0x20, device 0x4018),
40 MHz-kristal, MAC `44:bd:8d:79:a4:5c`. Er zijn geen aanwijzingen voor secure boot of
flash-encryptie: de image is leesbaar en de bootlog meldt niets.

### Partitietabel (Arduino-standaard `default_16MB.csv`)
| Naam | Offset | Grootte | Inhoud |
|---|---|---|---|
| nvs | 0x9000 | 20 KB | namespace `settings`, key `volume` (u8) |
| otadata | 0xe000 | 8 KB | actieve slot = **app0** |
| app0 | 0x10000 | 6400 KB | originele firmware (~2.9 MB gebruikt) |
| app1 | 0x650000 | 6400 KB | leeg (OTA-slot, ongebruikt) |
| spiffs | 0xc90000 | 3456 KB | leeg (alles 0xFF) |
| coredump | 0xff0000 | 64 KB | — |

Alle assets (afbeeldingen, fonts, animaties) zitten dus **in de app-binary zelf** en niet in een
bestandssysteem.

### Framework en libraries
- **PlatformIO**, board `esp32-s3-devkitc-1`, **Arduino-ESP32 2.0.x** op ESP-IDF v4.4.7, gebouwd
  op Windows
- **LVGL 8.x**, met de widgets arc, roller, dropdown, menu, chart, textarea, img en png. De
  map `src/ui/` met `ui_theme_manager.c` en `_ui_local_style_*` wijst op een UI uit
  **SquareLine Studio**.
- **GFX Library for Arduino** (`Arduino_ESP32QSPI`) voor het CO5300-display
- **SensorLib** (lewisxhe) voor touch en IMU. Bij het opstarten probeert de firmware achter elkaar
  CST226, CST816 en CST92xx; de laatste is de echte chip, de CST9217.
- **XPowersLib** voor de AXP2101 (accu en laden)
- **ES8311 + I2S**: de speaker wordt echt gebruikt (volume-instelling in NVS)
- **QMI8658**-IMU: wordt geïnitialiseerd ("QMI8658 initialized!")
- `Preferences` (NVS) en `Wire` (I2C)

### Niet gebruikt
- **Geen Wi-Fi en geen BLE**: er zijn geen sporen van `esp_wifi_init`, NimBLE/Bluedroid of SNTP.
  De firmware is dus echt offline. De HTTP-foutnamen in de binary komen uit de standaard
  `esp_err`-tabel.
- Geen SPIFFS/LittleFS en geen SD-kaart

### Bootlog (115200 baud via USB-CDC)
De log toont een reset via USB, laadt app0 en geeft daarna een paar onschuldige foutmeldingen
(`Wire setPins`, touch-chip-ID mismatch) tijdens het zoeken naar de juiste touch-driver. Hij eindigt
met `QMI8658 initialized!`.

## Openstaande vragen
- Waarvoor wordt de IMU gebruikt: tikken, schudden of kantelen? Dat is te achterhalen door
  ermee te testen.
- Welke geluiden speelt de speaker af?
