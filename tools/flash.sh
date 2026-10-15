#!/usr/bin/env bash
# Bouwt en flasht de deskbuddy-firmware. Overschrijft wat er op het apparaat staat!
#   tools/flash.sh          -> bouwen + flashen (vraagt eerst om bevestiging)
#   tools/flash.sh --yes    -> zonder vraag
set -euo pipefail
source "$(dirname "$0")/env.sh"
if ! ls "$ROOT"/backups/*.bin >/dev/null 2>&1; then
  echo "Geen backup in backups/. Run eerst tools/backup-flash.sh" >&2
  exit 1
fi
if [ "${1:-}" != "--yes" ]; then
  read -r -p "Firmware op $PORT overschrijven? [y/N] " ok
  [ "$ok" = "y" ] || { echo "Afgebroken."; exit 1; }
fi
cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e esp32 -t upload --upload-port "$PORT"
