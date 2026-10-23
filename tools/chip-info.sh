#!/usr/bin/env bash
# Reads chip, MAC and flash info (read-only, changes nothing).
set -euo pipefail
source "$(dirname "$0")/env.sh"
"$ESPTOOL" --chip esp32s3 --port "$PORT" flash-id
