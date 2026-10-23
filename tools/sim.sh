#!/usr/bin/env bash
# Builds the PC simulator and renders previews to sim-out/:
#   tools/sim.sh            -> PNG per emotion + contact sheet + demo.gif
#   tools/sim.sh emotions   -> only the PNGs
#   tools/sim.sh check      -> one overview per character of all emotions and actions (sim-out/check_NN.png)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/sim-out"
MODE="${1:-all}"
cd "$ROOT/firmware"
"$ROOT/.venv/bin/pio" run -e sim -s
BIN="$ROOT/firmware/.pio/build/sim/program"
rm -rf "$OUT" && mkdir -p "$OUT/frames"

if [ "$MODE" = "check" ]; then
  mkdir -p "$OUT/raw"
  "$BIN" "$OUT/raw" check
  for n in $(ls "$OUT/raw" | sed -n 's/^check_\([0-9][0-9]\)_.*/\1/p' | sort -u); do
    ffmpeg -loglevel error -y -pattern_type glob -i "$OUT/raw/check_${n}_*.ppm" \
      -filter_complex "scale=200:200,tile=6x4:padding=4:color=0x303030" -frames:v 1 "$OUT/check_$n.png"
  done
  rm -rf "$OUT/raw" "$OUT/frames"
  ls -1 "$OUT"
  exit 0
fi

"$BIN" "$OUT" emotions
for f in "$OUT"/*.ppm; do ffmpeg -loglevel error -y -i "$f" "${f%.ppm}.png" && rm "$f"; done
ffmpeg -loglevel error -y -pattern_type glob -i "$OUT/emotion_*.png" \
  -filter_complex "scale=233:233,tile=4x3:padding=6:color=0x303030" -frames:v 1 "$OUT/emotions.png"
ffmpeg -loglevel error -y -pattern_type glob -i "$OUT/mascot_*.png" \
  -filter_complex "scale=233:233,tile=4x3:padding=6:color=0x303030" -frames:v 1 "$OUT/mascot.png"

if [ "$MODE" != "emotions" ]; then
  "$BIN" "$OUT/frames" demo
  ffmpeg -loglevel error -y -framerate 30 -i "$OUT/frames/frame_%05d.ppm" \
    -vf "scale=300:300:flags=lanczos,split[a][b];[a]palettegen=max_colors=64[p];[b][p]paletteuse" "$OUT/demo.gif"
  ffmpeg -loglevel error -y -framerate 30 -i "$OUT/frames/frame_%05d.ppm" -i "$OUT/frames/demo.wav" \
    -pix_fmt yuv420p -c:a aac -shortest "$OUT/demo.mp4"
  mv "$OUT/frames/demo.wav" "$OUT/demo.wav"
  rm -rf "$OUT/frames"
fi
ls -1 "$OUT"
