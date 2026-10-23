#!/usr/bin/env bash
# Web emulator: the deskbuddy firmware (App, face, commands, web API) on the PC, operated in the
# browser, with the web interface or the PWA next to it (simulated Bluetooth). See docs/firmware.md.
#   tools/webemu.sh            -> http://localhost:8765/emu
#   tools/webemu.sh 9000       -> different port
#   tools/webemu.sh --reset    -> first clear the saved settings/profiles
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
# A broken build cache (e.g. "cannot find main.o" when linking) recovers itself: rebuild once from scratch.
if ! "$ROOT/.venv/bin/pio" run -e webemu -s; then
  echo "Build failed; cleaning the emulator build directory and trying once more..." >&2
  rm -rf "${ROOT:?}/firmware/.pio/build/webemu"
  "$ROOT/.venv/bin/pio" run -e webemu -s
fi
if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
  echo "Port $PORT is already in use (is an emulator already running? stop it with: pkill -x program)." >&2
  echo "Or choose a different port: tools/webemu.sh 9000" >&2
  exit 1
fi
exec "$ROOT/firmware/.pio/build/webemu/program" "$ROOT" "$PORT"
