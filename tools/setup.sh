#!/usr/bin/env bash
# Creates a local Python venv with esptool, pyserial and PlatformIO in .venv/
set -euo pipefail
cd "$(dirname "$0")/.."
# PlatformIO with the ESP32 platform (pioarduino) only works with Python 3.10-3.14: pick one of those if available.
PY=""
for v in 3.14 3.13 3.12 3.11 3.10; do
  if command -v "python$v" >/dev/null; then PY="python$v"; break; fi
done
PY="${PY:-python3}"
# Existing venv with a different (unsupported) Python: recreate it
if [ -x .venv/bin/python3 ] && [ "$(.venv/bin/python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')" != "$("$PY" -c 'import sys; print("%d.%d" % sys.version_info[:2])')" ]; then
  echo "Venv uses a different Python; recreating it with $PY"
  rm -rf .venv
fi
"$PY" -m venv .venv
.venv/bin/pip install --quiet --upgrade pip esptool pyserial platformio
echo "Done. esptool: $(.venv/bin/esptool version | tail -1)"
if ! id -nG | grep -qw dialout; then
  echo "NOTE: you are not in the 'dialout' group. Run:  sudo usermod -aG dialout \$USER  and log in again."
fi
