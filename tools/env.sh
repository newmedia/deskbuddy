# Shared settings; sourced by the other scripts.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${PORT:-$(ls /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_*-if00 2>/dev/null | head -1)}"
ESPTOOL="$ROOT/.venv/bin/esptool"
[ -n "$PORT" ] || { echo "No ESP32-S3 found on USB." >&2; exit 1; }
[ -x "$ESPTOOL" ] || { echo "Run tools/setup.sh first" >&2; exit 1; }
