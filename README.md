<p align="center"><img src="firmware/web/icon.svg" alt="deskbuddy.dev" width="128" height="128"></p>

# deskbuddy.dev

<p align="center">
  <a href="https://deskbuddy.dev"><b>deskbuddy.dev</b></a> ·
  <a href="https://newmedia.github.io/deskbuddy/flash/"><b>Firmware installeren</b></a> ·
  <a href="https://newmedia.github.io/deskbuddy/">Web-app (PWA)</a> ·
  <a href="https://github.com/newmedia/deskbuddy/releases">Alle versies</a>
</p>

Een desk-robot met een rond AMOLED-scherm, gebouwd op een
[Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm)
in een 3D-geprinte behuizing. Op het bord staat nu nog de originele closed-source firmware;
De eigen deskbuddy-firmware staat in `firmware/`.

## Installeren
Zonder iets te bouwen: open **https://newmedia.github.io/deskbuddy/flash/** in Chrome of Edge, sluit
het bord via USB aan en klik op *Verbind en installeer*. Let op: dit vervangt de firmware op het
apparaat (maak eerst een backup als je de originele wilt bewaren).

## Documentatie
- [docs/hardware.md](docs/hardware.md): het bord, de chips, de pin-mapping en hoe het zich op USB meldt
- [docs/original-firmware.md](docs/original-firmware.md): wat de originele firmware doet en wat nog open staat
- [docs/firmware.md](docs/firmware.md): de eigen deskbuddy-firmware: bediening, opbouw, bouwen en flashen
- [docs/regressietest.md](docs/regressietest.md): de automatische regressietest op de web-emulator gebruiken en uitbreiden
- [docs/multi-buddy.md](docs/multi-buddy.md): idee en ontwerp voor meerdere buddies die op elkaar reageren
- [docs/plan.md](docs/plan.md): plan voor eigen deskbuddy-firmware, web/PWA, desktop-app en HA

## Snel starten
```bash
sudo usermod -aG dialout $USER   # eenmalig, daarna opnieuw inloggen
tools/setup.sh                   # venv met esptool + pyserial
tools/chip-info.sh               # chip/flash-info (alleen lezen)
tools/backup-flash.sh            # volledige flash-backup naar backups/
tools/monitor.sh                 # serial log bekijken
tools/sim.sh                     # firmware-preview op de PC (sim-out/demo.gif)
tools/webemu.sh                  # web-emulator: http://localhost:8765/emu
tools/webemu-test.sh             # regressietest zonder apparaat (exitcode 0 = goed)
tools/flash.sh                   # deskbuddy-firmware flashen (vraagt bevestiging)
```

Het apparaat verschijnt als `303a:1001` (Espressif USB JTAG/serial) op `/dev/ttyACM0`.
Stel `PORT=...` in om een andere poort te gebruiken.

> Maak altijd een backup vóór je flasht. De originele firmware is niet open source.
