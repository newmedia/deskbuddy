# Gedeelde instellingen; wordt gesourced door de andere scripts.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${PORT:-$(ls /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00 2>/dev/null | head -1)}"
ESPTOOL="$ROOT/.venv/bin/esptool"
[ -n "$PORT" ] || { echo "Geen ESP32-S3 gevonden op USB." >&2; exit 1; }
[ -x "$ESPTOOL" ] || { echo "Run eerst tools/setup.sh" >&2; exit 1; }
