#!/usr/bin/env bash
# Maakt een lokale Python-venv met esptool, pyserial en PlatformIO in .venv/
set -euo pipefail
cd "$(dirname "$0")/.."
# PlatformIO met het ESP32-platform (pioarduino) werkt alleen met Python 3.10-3.14: kies die als hij er is.
PY=""
for v in 3.14 3.13 3.12 3.11 3.10; do
  if command -v "python$v" >/dev/null; then PY="python$v"; break; fi
done
PY="${PY:-python3}"
# Bestaande venv met een andere (niet ondersteunde) Python: opnieuw aanmaken
if [ -x .venv/bin/python3 ] && [ "$(.venv/bin/python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')" != "$("$PY" -c 'import sys; print("%d.%d" % sys.version_info[:2])')" ]; then
  echo "Venv gebruikt een andere Python; opnieuw aanmaken met $PY"
  rm -rf .venv
fi
"$PY" -m venv .venv
.venv/bin/pip install --quiet --upgrade pip esptool pyserial platformio
echo "Klaar. esptool: $(.venv/bin/esptool version | tail -1)"
if ! id -nG | grep -qw dialout; then
  echo "LET OP: je zit niet in de 'dialout'-groep. Run:  sudo usermod -aG dialout \$USER  en log opnieuw in."
fi
