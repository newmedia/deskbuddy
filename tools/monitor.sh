#!/usr/bin/env bash
# Shows the device's serial log (Ctrl+C to stop).
set -euo pipefail
source "$(dirname "$0")/env.sh"
exec "$ROOT/.venv/bin/python" -m serial.tools.miniterm --raw "$PORT" 115200
