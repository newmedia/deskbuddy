#!/usr/bin/env bash
# Bouwt en start de interactieve emulator (vereist: sudo apt install libsdl2-dev).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e emu -s
exec "$ROOT/firmware/.pio/build/emu/program"
