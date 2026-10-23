#!/usr/bin/env bash
# Builds and flashes the deskbuddy firmware. Overwrites what is on the device!
#   tools/flash.sh          -> build + flash (asks for confirmation first)
#   tools/flash.sh --yes    -> without asking
set -euo pipefail
source "$(dirname "$0")/env.sh"
if ! ls "$ROOT"/backups/*.bin >/dev/null 2>&1; then
  echo "No backup in backups/. Run tools/backup-flash.sh first" >&2
  exit 1
fi
if [ "${1:-}" != "--yes" ]; then
  read -r -p "Overwrite firmware on $PORT? [y/N] " ok
  [ "$ok" = "y" ] || { echo "Aborted."; exit 1; }
fi
cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e esp32 -t upload --upload-port "$PORT"
