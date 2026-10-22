#!/usr/bin/env bash
# Publiceert de installatiepagina (firmware via USB flashen vanuit de browser) naar gh-pages:
#   https://newmedia.github.io/deskbuddy/flash/
# Alle GitHub-releases komen op de pagina, zodat je zelf een versie kunt kiezen. Per versie wordt
# firmware.bin uit de release gehaald (gecontroleerd met de sha256 uit die release) en komt er een
# eigen manifest. Bootloader, partitietabel en boot_app0 komen uit de lokale build (.pio/build/esp32);
# die staan vast voor deze borduitvoering en worden door alle versies gedeeld.
# Draai dit na elke release (tools/release.sh). Bestaande versies worden hergebruikt, niet opnieuw geladen.
#
#   tools/publish-flash.sh
#   DRYRUN=/tmp/site tools/publish-flash.sh   (proefdraai: niets publiceren)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="newmedia/deskbuddy"
cd "$ROOT"

command -v gh >/dev/null || { echo "gh (GitHub CLI) ontbreekt." >&2; exit 1; }
BUILD="$ROOT/firmware/.pio/build/esp32"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
for f in "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$BOOT_APP0"; do
  [ -f "$f" ] || { echo "Ontbreekt: $f (bouw eerst: tools/flash.sh of pio run -e esp32)" >&2; exit 1; }
done

TMP="$(mktemp -d)"; DL="$(mktemp -d)"
trap 'git worktree remove --force "$TMP" >/dev/null 2>&1 || true; rm -rf "$TMP" "$DL"' EXIT

git fetch -q origin gh-pages 2>/dev/null || true
git worktree add -q -B gh-pages "$TMP" origin/gh-pages
mkdir -p "$TMP/flash"

# Releases (nieuwste eerst), zonder concepten en pre-releases
gh release list -R "$REPO" --limit 100 --json tagName,publishedAt,isPrerelease,isDraft \
  --jq '[.[]|select(.isPrerelease|not)|select(.isDraft|not)]' > "$DL/releases.json"
TAGS="$(python3 -c "import json;print(' '.join(r['tagName'] for r in json.load(open('$DL/releases.json'))))")"
[ -n "$TAGS" ] || { echo "Geen releases gevonden." >&2; exit 1; }

for TAG in $TAGS; do
  V="${TAG#v}"
  if [ -f "$TMP/flash/$V/firmware.bin" ] && [ -f "$TMP/flash/$V/manifest.json" ]; then
    echo "== $TAG staat er al"
    continue
  fi
  echo "== $TAG downloaden"
  mkdir -p "$DL/$V"
  gh release download "$TAG" -R "$REPO" -D "$DL/$V" -p firmware.bin -p firmware.bin.sha256
  (cd "$DL/$V" && sha256sum -c firmware.bin.sha256 >/dev/null) || { echo "Checksum van $TAG klopt niet." >&2; exit 1; }
  grep -aq "$V" "$DL/$V/firmware.bin" || { echo "Versie $V niet in firmware.bin van $TAG gevonden." >&2; exit 1; }
  mkdir -p "$TMP/flash/$V"
  cp "$DL/$V/firmware.bin" "$TMP/flash/$V/firmware.bin"
  # Offsets (decimaal): 0x0 bootloader, 0x8000 partitietabel, 0xE000 boot_app0, 0x10000 app. De
  # NVS-partitie (0x9000, instellingen en Wi-Fi) blijft zo onaangeroerd. De gedeelde delen staan één map hoger.
  cat > "$TMP/flash/$V/manifest.json" <<JSON
{
  "name": "deskbuddy",
  "version": "$V",
  "new_install_prompt_erase": false,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "../bootloader.bin", "offset": 0 },
        { "path": "../partitions.bin", "offset": 32768 },
        { "path": "../boot_app0.bin", "offset": 57344 },
        { "path": "firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
JSON
done

# Lijst voor de pagina (nieuwste eerst; de omschrijving komt uit CHANGELOG.md: eerste regel onder de kop)
python3 - "$DL/releases.json" "$ROOT/CHANGELOG.md" "$TMP/flash/versions.json" <<'PY'
import json, re, sys
rel = json.load(open(sys.argv[1]))
notes, cur, par = {}, None, []
for line in open(sys.argv[2], encoding="utf-8"):
    m = re.match(r"## v(\S+)", line)
    if m:
        cur, par = m.group(1), []
        continue
    if cur and cur not in notes:
        if line.strip():
            par.append(line.strip())
        elif par:  # eerste alinea (tot de eerste lege regel) is de samenvatting
            notes[cur] = " ".join(par)
if cur and cur not in notes and par:
    notes[cur] = " ".join(par)
out = [{"version": r["tagName"][1:], "date": r["publishedAt"][:10], "note": notes.get(r["tagName"][1:], "")} for r in rel]
json.dump({"versions": out}, open(sys.argv[3], "w"), ensure_ascii=False, indent=1)
PY

cp firmware/web/flash/index.html "$TMP/flash/index.html"
cp "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$TMP/flash/"
cp "$BOOT_APP0" "$TMP/flash/boot_app0.bin"
# Oude losse bestanden uit de eerste opzet (alleen de nieuwste versie) opruimen
rm -f "$TMP/flash/firmware.bin" "$TMP/flash/manifest.json"
touch "$TMP/.nojekyll"
# Proefdraaien: DRYRUN=<map> zet het resultaat in die map i.p.v. te publiceren (om lokaal te testen)
if [ -n "${DRYRUN:-}" ]; then
  mkdir -p "$DRYRUN" && cp -r "$TMP/flash" "$DRYRUN/" && cp firmware/web/icon.svg firmware/web/*.png "$DRYRUN/"
  echo "Proefdraai klaar in $DRYRUN"
  exit 0
fi
git -C "$TMP" add -A flash .nojekyll
if git -C "$TMP" diff --cached --quiet; then
  echo "Installatiepagina is al up-to-date."
  exit 0
fi
git -C "$TMP" commit -q -m "Installatiepagina bijgewerkt (nieuwste: $(echo $TAGS | cut -d' ' -f1))"
git -C "$TMP" push -q origin gh-pages
echo "Gepubliceerd: https://newmedia.github.io/deskbuddy/flash/ (kan een minuut duren)"
