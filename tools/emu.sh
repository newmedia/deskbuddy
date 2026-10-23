#!/usr/bin/env bash
# Builds and starts the interactive emulator (requires: sudo apt install libsdl2-dev).
#   tools/emu.sh        -> Original (round 466x466)
#   tools/emu.sh --xl   -> XL (square 480x480)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENV=emu
[ "${1:-}" = "--xl" ] && ENV=emu-xl
cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e "$ENV" -s
exec "$ROOT/firmware/.pio/build/$ENV/program"
