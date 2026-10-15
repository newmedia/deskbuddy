# deskbuddy

Een desk-robot met een rond AMOLED-scherm, gebouwd op een
[Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm)
in een 3D-geprinte behuizing. Op het bord staat nu nog de originele closed-source firmware;
doel is eigen deskbuddy-firmware.

## Documentatie
- [docs/hardware.md](docs/hardware.md): het bord, de chips, de pin-mapping en hoe het zich op USB meldt
- [docs/original-firmware.md](docs/original-firmware.md): wat de originele firmware doet en wat nog open staat
- [docs/plan.md](docs/plan.md): plan voor eigen deskbuddy-firmware, web/PWA, desktop-app en HA

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

> Maak altijd een backup vóór je flasht. De originele firmware is niet open source.
