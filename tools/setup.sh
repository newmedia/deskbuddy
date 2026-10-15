#!/usr/bin/env bash
# Maakt een lokale Python-venv met esptool, pyserial en PlatformIO in .venv/
set -euo pipefail
cd "$(dirname "$0")/.."
python3 -m venv .venv
.venv/bin/pip install --quiet --upgrade pip esptool pyserial platformio
echo "Klaar. esptool: $(.venv/bin/esptool version | tail -1)"
if ! id -nG | grep -qw dialout; then
  echo "LET OP: je zit niet in de 'dialout'-groep. Run:  sudo usermod -aG dialout \$USER  en log opnieuw in."
fi
