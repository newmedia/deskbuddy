---
name: esp-device
description: Inspect, back up, monitor or flash the Deskimon ESP32-S3 (Waveshare AMOLED 1.75) on USB. Use for "lees de chip", "backup", "serial log", "flash firmware", "wat draait er op".
---

# ESP32-S3 Deskimon workflow

1. **Check aanwezigheid**: `lsusb | grep 303a:1001`. Ontbreekt het apparaat, vraag dan of het is
   aangesloten en aanstaat (PWR-knop).
2. **Check rechten**: `id -nG | grep -w dialout`. Ontbreekt de groep, geef de gebruiker
   `sudo usermod -aG dialout $USER` (opnieuw inloggen). Voer het niet zelf uit.
3. **Tooling**: bestaat `.venv/bin/esptool` niet, run dan `tools/setup.sh`.
4. **Alleen lezen** (veilig, zonder te vragen):
   - `tools/chip-info.sh`: chip, MAC, flash-grootte
   - `tools/backup-flash.sh`: dump van 16 MB plus partitietabel
   - `timeout 10 tools/monitor.sh`: bootlog. Een reset via esptool levert de volledige bootlog op.
5. **Schrijven** (flashen of wissen): alleen na expliciete bevestiging van de gebruiker **en** met een
   bestaande backup in `backups/`. Herstellen gaat zo:
   `.venv/bin/esptool --chip esp32s3 --port $PORT write-flash 0 backups/<file>.bin`
6. Werkt de verbinding niet, zet dan de downloadmodus aan: houd BOOT ingedrukt, tik PWR of reset aan
   en laat BOOT los.

Voeg nieuwe bevindingen (framework, partities, gebruik van Wi-Fi) toe aan `docs/deskimon-software.md`.
