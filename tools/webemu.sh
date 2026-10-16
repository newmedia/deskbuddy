#!/usr/bin/env bash
# Web-emulator: de deskbuddy-firmware (App, gezicht, commando's, web-API) op de PC, te bedienen in de
# browser, met ernaast de webinterface of de PWA (nagebootste Bluetooth). Zie docs/firmware.md.
#   tools/webemu.sh            -> http://localhost:8765/emu
#   tools/webemu.sh 9000       -> andere poort
#   tools/webemu.sh --reset    -> eerst de bewaarde instellingen/profielen wissen
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-8765}"
for a in "$@"; do
  case "$a" in
    --reset) rm -rf "$ROOT/firmware/.pio/webemu-state" ;;
    *) PORT="$a" ;;
  esac
done
cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e webemu -s
exec "$ROOT/firmware/.pio/build/webemu/program" "$ROOT" "$PORT"
