#!/usr/bin/env bash
# Leest chip-, MAC- en flash-info (alleen lezen, wijzigt niets).
set -euo pipefail
source "$(dirname "$0")/env.sh"
"$ESPTOOL" --chip esp32s3 --port "$PORT" flash-id
