#!/usr/bin/env bash
# Publiceert de installatiepagina (firmware via USB flashen vanuit de browser) naar gh-pages:
#   https://newmedia.github.io/deskbuddy/flash/
# De firmware zelf komt uit de nieuwste GitHub-release (firmware.bin, gecontroleerd met de sha256 uit
# die release). Bootloader, partitietabel en boot_app0 komen uit de lokale build (.pio/build/esp32);
# die staan vast voor deze borduitvoering. Draai dit na elke release (tools/release.sh).
#
#   tools/publish-flash.sh            -> nieuwste release
#   tools/publish-flash.sh 1.1.0      -> een specifieke release
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="newmedia/deskbuddy"
cd "$ROOT"

command -v gh >/dev/null || { echo "gh (GitHub CLI) ontbreekt." >&2; exit 1; }
TAG="${1:+v${1#v}}"
[ -n "$TAG" ] || TAG="$(gh release view -R "$REPO" --json tagName --jq .tagName)"
VERSION="${TAG#v}"
BUILD="$ROOT/firmware/.pio/build/esp32"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
for f in "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$BOOT_APP0"; do
  [ -f "$f" ] || { echo "Ontbreekt: $f (bouw eerst: tools/flash.sh of pio run -e esp32)" >&2; exit 1; }
done

TMP="$(mktemp -d)"; DL="$(mktemp -d)"
trap 'git worktree remove --force "$TMP" >/dev/null 2>&1 || true; rm -rf "$TMP" "$DL"' EXIT

echo "== $TAG downloaden"
gh release download "$TAG" -R "$REPO" -D "$DL" -p firmware.bin -p firmware.bin.sha256
(cd "$DL" && sha256sum -c firmware.bin.sha256 >/dev/null) || { echo "Checksum van firmware.bin klopt niet." >&2; exit 1; }
grep -aq "$VERSION" "$DL/firmware.bin" || { echo "Versie $VERSION niet in firmware.bin gevonden." >&2; exit 1; }

git fetch -q origin gh-pages 2>/dev/null || true
git worktree add -q -B gh-pages "$TMP" origin/gh-pages

mkdir -p "$TMP/flash"
cp firmware/web/flash/index.html "$TMP/flash/index.html"
cp "$DL/firmware.bin" "$TMP/flash/firmware.bin"
cp "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$TMP/flash/"
cp "$BOOT_APP0" "$TMP/flash/boot_app0.bin"
# Offsets (decimaal): 0x0 bootloader, 0x8000 partitietabel, 0xE000 boot_app0, 0x10000 app. De NVS-partitie
# (0x9000, instellingen en Wi-Fi) blijft zo onaangeroerd.
cat > "$TMP/flash/manifest.json" <<JSON
{
  "name": "deskbuddy",
  "version": "$VERSION",
  "new_install_prompt_erase": false,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "bootloader.bin", "offset": 0 },
        { "path": "partitions.bin", "offset": 32768 },
        { "path": "boot_app0.bin", "offset": 57344 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
JSON
touch "$TMP/.nojekyll"
git -C "$TMP" add -A flash .nojekyll
if git -C "$TMP" diff --cached --quiet; then
  echo "Installatiepagina is al up-to-date ($TAG)."
  exit 0
fi
git -C "$TMP" commit -q -m "Installatiepagina bijgewerkt: $TAG"
git -C "$TMP" push -q origin gh-pages
echo "Gepubliceerd: https://newmedia.github.io/deskbuddy/flash/ ($TAG; kan een minuut duren)"
