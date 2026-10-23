#!/usr/bin/env bash
# Web emulator: the deskbuddy firmware (App, face, commands, web API) on the PC, operated in the
# browser, with the web interface or the PWA next to it (simulated Bluetooth). See docs/firmware.md.
#   tools/webemu.sh            -> http://localhost:8765/emu
#   tools/webemu.sh 9000       -> different port
#   tools/webemu.sh --reset    -> first clear the saved settings/profiles
#   tools/webemu.sh --xl       -> the XL (2.16", square 480x480) on http://localhost:8766/emu, own settings
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV=webemu
for a in "$@"; do [ "$a" = "--xl" ] && ENV=webemu-xl; done
if [ "$ENV" = webemu-xl ]; then DEFPORT=8766; DEFSTATE=webemu-xl-state; else DEFPORT=8765; DEFSTATE=webemu-state; fi
PORT="${PORT:-$DEFPORT}"
export WEBEMU_STATE="${WEBEMU_STATE:-$ROOT/firmware/.pio/$DEFSTATE}"
for a in "$@"; do
  case "$a" in
    --reset) rm -rf "$WEBEMU_STATE" ;;
    --xl) ;;
    *) PORT="$a" ;;
  esac
done
cd "$ROOT/firmware"
# A broken build cache (e.g. "cannot find main.o" when linking) recovers itself: rebuild once from scratch.
if ! "$ROOT/.venv/bin/pio" run -e "$ENV" -s; then
  echo "Build failed; cleaning the emulator build directory and trying once more..." >&2
  rm -rf "${ROOT:?}/firmware/.pio/build/$ENV"
  "$ROOT/.venv/bin/pio" run -e "$ENV" -s
fi
if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
  echo "Port $PORT is already in use (is an emulator already running? stop it with: pkill -x program)." >&2
  echo "Or choose a different port: tools/webemu.sh 9000" >&2
  exit 1
fi
exec "$ROOT/firmware/.pio/build/$ENV/program" "$ROOT" "$PORT"
