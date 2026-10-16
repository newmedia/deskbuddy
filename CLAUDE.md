# CLAUDE.md

Project rond een deskbuddy desk-robot. Hardware: Waveshare ESP32-S3-Touch-AMOLED-1.75 (ESP32-S3R8,
16 MB flash, 466×466 AMOLED met CO5300 + CST9217-touch). Details en pins staan in `docs/hardware.md`.
Taal van docs en commits: Nederlands.

## Apparaat
- USB `303a:1001`, poort `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00`
- Voor toegang tot de poort moet de gebruiker in `dialout` zitten. Sudo nooit zelf uitvoeren; vraag
  het de gebruiker. Is de groep al toegevoegd maar nog niet actief in de shell, gebruik dan `sg dialout -c "…"`.
- Gebruik de scripts in `tools/` (esptool v5 in `.venv/`, commando's met streepjes, zoals `read-flash`)

## Code
- `firmware/`: PlatformIO, met env `esp32` (apparaat), `sim` (headless PC-simulator), `emu`
  (interactieve SDL-emulator, `tools/emu.sh`) en `webemu` (web-emulator met webinterface en PWA,
  `tools/webemu.sh` -> http://localhost:8765/emu). Uitleg in `docs/firmware.md`.
- Draai `tools/webemu-test.sh` (regressietest, `docs/regressietest.md`) na wijzigingen in de
  firmware-logica of web-app, en altijd vóór een release of flashverzoek.
- Test functionaliteit en web-app/PWA eerst in de web-emulator (bijv. via de ingebouwde browser,
  config `webemu` in `.claude/launch.json`); prestaties meet je op het apparaat.
- Hardware-onafhankelijke logica hoort in `firmware/lib/buddy/` (alleen LVGL), zodat de simulator
  hem ook draait. Hardware hoort in `firmware/src/board/`.
- Controleer UI-wijzigingen met `tools/sim.sh` en bekijk de PNG's in `sim-out/` voordat je om
  flashen vraagt.
- Plan en fases staan in `docs/plan.md`. Navigatie: veeg omlaag = info (Wi-Fi/BLE/geluid/skin),
  veeg omhoog = klok.
- Start de emulator ontkoppeld met `setsid … &` en stop hem met `pkill -x program`. Gebruik geen
  `pkill -f`, want die matcht ook je eigen shell.

## Regels
- **Nooit flashen, wissen of `write-flash` uitvoeren zonder expliciete toestemming** en zonder dat er
  een backup in `backups/` staat. Op de chip staat de enige kopie van de closed-source
  originele firmware.
- `backups/` staat in `.gitignore` (IP van de oorspronkelijke maker). Commit of publiceer dumps nooit.
- Lees-acties (chip-info, read-flash, monitor) zijn veilig.

## Skill
`.claude/skills/esp-device/` bevat de workflow voor dit apparaat: inspecteren, backup maken, flashen.
