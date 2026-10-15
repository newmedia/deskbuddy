# deskbuddy

Werkmap rond een **Deskimon** desk-robot (Orbit & Sprout, van
[Creative Chance](https://creative-chance.com/en-eur/products/orbit-sprout-deskimon)).
De robot bestaat uit een 3D-geprinte behuizing om een
[Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm)
met closed-source Deskimon-firmware.

## Documentatie
- [docs/hardware.md](docs/hardware.md): het bord, de chips, de pin-mapping en hoe het zich op USB meldt
- [docs/deskimon-software.md](docs/deskimon-software.md): wat de Deskimon-firmware doet en wat nog open staat

## Snel starten
```bash
sudo usermod -aG dialout $USER   # eenmalig, daarna opnieuw inloggen
tools/setup.sh                   # venv met esptool + pyserial
tools/chip-info.sh               # chip/flash-info (alleen lezen)
tools/backup-flash.sh            # volledige flash-backup naar backups/
tools/monitor.sh                 # serial log bekijken
```

Het apparaat verschijnt als `303a:1001` (Espressif USB JTAG/serial) op `/dev/ttyACM0`.
Stel `PORT=...` in om een andere poort te gebruiken.

> Maak altijd een backup vóór je flasht. De Deskimon-firmware is niet open source.
