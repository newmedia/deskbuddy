#!/usr/bin/env bash
# Toont de serial log van het apparaat (Ctrl+C om te stoppen).
set -euo pipefail
source "$(dirname "$0")/env.sh"
exec "$ROOT/.venv/bin/python" -m serial.tools.miniterm --raw "$PORT" 115200
