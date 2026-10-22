#!/usr/bin/env bash
# Web-emulator: de deskbuddy-firmware (App, gezicht, commando's, web-API) op de PC, te bedienen in de
# browser, met ernaast de webinterface of de PWA (nagebootste Bluetooth). Zie docs/firmware.md.
#   tools/webemu.sh            -> http://localhost:8765/emu
#   tools/webemu.sh 9000       -> andere poort
#   tools/webemu.sh --reset    -> eerst de bewaarde instellingen/profielen wissen
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${PORT:-8765}"
export WEBEMU_STATE="${WEBEMU_STATE:-$ROOT/firmware/.pio/webemu-state}"
for a in "$@"; do
  case "$a" in
    --reset) rm -rf "$WEBEMU_STATE" ;;
    *) PORT="$a" ;;
  esac
done
cd "$ROOT/firmware"
# Een kapotte bouwcache (bijv. "cannot find main.o" bij het linken) herstelt zichzelf: eenmaal schoon opnieuw bouwen.
if ! "$ROOT/.venv/bin/pio" run -e webemu -s; then
  echo "Bouwen mislukt; de bouwmap van de emulator wordt opgeruimd en het wordt nog eens geprobeerd..." >&2
  rm -rf "${ROOT:?}/firmware/.pio/build/webemu"
  "$ROOT/.venv/bin/pio" run -e webemu -s
fi
if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
  echo "Poort $PORT is al in gebruik (draait er al een emulator? stop die met: pkill -x program)." >&2
  echo "Of kies een andere poort: tools/webemu.sh 9000" >&2
  exit 1
fi
exec "$ROOT/firmware/.pio/build/webemu/program" "$ROOT" "$PORT"
