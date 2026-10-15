#!/usr/bin/env bash
# Volledige 16 MB flash-dump naar backups/ (alleen lezen). Doe dit VOOR je ooit iets flasht.
set -euo pipefail
source "$(dirname "$0")/env.sh"
mkdir -p "$ROOT/backups"
OUT="$ROOT/backups/original-firmware-$(date +%Y%m%d-%H%M%S).bin"
"$ESPTOOL" --chip esp32s3 --port "$PORT" --baud 921600 read-flash 0 0x1000000 "$OUT"
sha256sum "$OUT" | tee "$OUT.sha256"
echo "Partitietabel (offset 0x8000):"
python3 -I -c '
import struct,sys
d=open(sys.argv[1],"rb").read()[0x8000:0x9000]
for i in range(0,len(d),32):
    e=d[i:i+32]
    if e[:2]!=b"\xaa\x50": break
    t,s,off,size=struct.unpack("<BBII",e[2:12]); name=e[12:28].split(b"\0")[0].decode()
    print(f"  {name:<16} type={t:#04x} sub={s:#04x} off={off:#010x} size={size:#x} ({size//1024} KB)")
' "$OUT"
