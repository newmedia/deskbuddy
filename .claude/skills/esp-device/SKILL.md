---
name: esp-device
description: Inspect, back up, monitor or flash the deskbuddy ESP32-S3 (Waveshare AMOLED 1.75) on USB. Use for "read the chip", "backup", "serial log", "flash firmware", "what is running on it".
---

# ESP32-S3 deskbuddy workflow

1. **Check presence**: `lsusb | grep 303a:1001`. If the device is missing, ask whether it is
   connected and powered on (PWR button).
2. **Check permissions**: `id -nG | grep -w dialout`. If the group is missing, give the user
   `sudo usermod -aG dialout $USER` (log in again). Do not run it yourself. If the user is in
   `getent group dialout` but the shell does not have the group yet, use `sg dialout -c "tools/…"`.
3. **Tooling**: if `.venv/bin/esptool` does not exist, run `tools/setup.sh`.
4. **Read-only** (safe, no need to ask):
   - `tools/chip-info.sh`: chip, MAC, flash size
   - `tools/backup-flash.sh`: dump of 16 MB plus partition table
   - `timeout 10 tools/monitor.sh`: boot log. A reset via esptool produces the full boot log.
5. **Writing** (flashing or erasing): only after explicit confirmation from the user **and** with an
   existing backup in `backups/`. Custom firmware: `tools/flash.sh --yes` (only after that confirmation).
   Restoring works like this:
   `.venv/bin/esptool --chip esp32s3 --port $PORT write-flash 0 backups/<file>.bin`
6. If the connection does not work, enable download mode: hold BOOT, tap PWR or reset,
   and release BOOT.

Add new findings (framework, partitions, Wi-Fi usage) to `docs/original-firmware.md`.
