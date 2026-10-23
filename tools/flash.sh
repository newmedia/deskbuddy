#!/usr/bin/env bash
# Builds and flashes the deskbuddy firmware. Overwrites what is on the device!
#   tools/flash.sh               -> Original (ESP32-S3-Touch-AMOLED-1.75): build + flash (asks first)
#   tools/flash.sh --xl          -> XL (ESP32-S3-Touch-AMOLED-2.16)
#   tools/flash.sh [--xl] --yes  -> without asking
# Both boards have the same chip, so esptool cannot tell them apart: choose the right one yourself.
# Requires a backup of this chip (its MAC in the file name, see tools/backup-flash.sh) in backups/.
set -euo pipefail
source "$(dirname "$0")/env.sh"
ENV=esp32
BOARD="Original (ESP32-S3-Touch-AMOLED-1.75)"
YES=0
for a in "$@"; do
  case "$a" in
    --xl) ENV=esp32-xl; BOARD="XL (ESP32-S3-Touch-AMOLED-2.16)" ;;
    --yes) YES=1 ;;
    *) echo "usage: tools/flash.sh [--xl] [--yes]" >&2; exit 1 ;;
  esac
done
# MAC from the port name (.../usb-Espressif_USB_JTAG_serial_debug_unit_28:84:85:90:78:40-if00)
MAC="$(basename "$PORT" | sed -n 's/.*_\(\([0-9A-Fa-f]\{2\}:\)\{5\}[0-9A-Fa-f]\{2\}\)-if00$/\1/p' | tr -d ':' | tr 'A-F' 'a-f')"
[ -n "$MAC" ] || { echo "Cannot read the MAC from the port name ($PORT)." >&2; exit 1; }
if ! ls "$ROOT"/backups/*"$MAC"*.bin >/dev/null 2>&1; then
  echo "No backup of this chip ($MAC) in backups/. Run tools/backup-flash.sh first." >&2
  exit 1
fi
if [ "$YES" != 1 ]; then
  read -r -p "Flash the $BOARD firmware onto $PORT (chip $MAC)? [y/N] " ok
  [ "$ok" = "y" ] || { echo "Aborted."; exit 1; }
fi
cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e "$ENV" -t upload --upload-port "$PORT"
