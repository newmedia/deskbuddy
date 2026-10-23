#!/usr/bin/env bash
# Publishes the install page (flash the firmware over USB from the browser) to gh-pages:
#   https://newmedia.github.io/deskbuddy/flash/
# All GitHub releases appear on the page so you can pick a board and a version yourself. For each
# version, firmware.bin (Original) and, when the release has it, firmware-xl.bin (XL) are fetched
# (verified with the sha256 from that release), each with a manifest of its own. Bootloader, partition table and boot_app0 come from the local build (.pio/build/esp32);
# these are fixed for this board variant and shared by all versions.
# Run this after every release (tools/release.sh). Existing versions are reused, not downloaded again.
#
#   tools/publish-flash.sh
#   DRYRUN=/tmp/site tools/publish-flash.sh   (dry run: publish nothing)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPO="newmedia/deskbuddy"
cd "$ROOT"

command -v gh >/dev/null || { echo "gh (GitHub CLI) is missing." >&2; exit 1; }
BUILD="$ROOT/firmware/.pio/build/esp32"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
for f in "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$BOOT_APP0"; do
  [ -f "$f" ] || { echo "Missing: $f (build first: tools/flash.sh or pio run -e esp32)" >&2; exit 1; }
done

TMP="$(mktemp -d)"; DL="$(mktemp -d)"
trap 'git worktree remove --force "$TMP" >/dev/null 2>&1 || true; rm -rf "$TMP" "$DL"' EXIT

git fetch -q origin gh-pages 2>/dev/null || true
git worktree add -q -B gh-pages "$TMP" origin/gh-pages
mkdir -p "$TMP/flash"

# Releases (newest first), without drafts and pre-releases
gh release list -R "$REPO" --limit 100 --json tagName,publishedAt,isPrerelease,isDraft \
  --jq '[.[]|select(.isPrerelease|not)|select(.isDraft|not)]' > "$DL/releases.json"
TAGS="$(python3 -c "import json;print(' '.join(r['tagName'] for r in json.load(open('$DL/releases.json'))))")"
[ -n "$TAGS" ] || { echo "No releases found." >&2; exit 1; }

# Manifest for ESP Web Tools: $1 = version dir, $2 = version, $3 = app file, $4 = manifest file.
# Offsets (decimal): 0x0 bootloader, 0x8000 partition table, 0xE000 boot_app0, 0x10000 app. The NVS
# partition (0x9000, settings and Wi-Fi) is thus left untouched. The shared parts are one directory up
# (the same for the Original and the XL).
manifest() {
  cat > "$1/$4" <<JSON
{
  "name": "deskbuddy",
  "version": "$2",
  "new_install_prompt_erase": false,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "../bootloader.bin", "offset": 0 },
        { "path": "../partitions.bin", "offset": 32768 },
        { "path": "../boot_app0.bin", "offset": 57344 },
        { "path": "$3", "offset": 65536 }
      ]
    }
  ]
}
JSON
}

# Per version: firmware.bin (Original) and, from the first release with XL support on, firmware-xl.bin
for TAG in $TAGS; do
  V="${TAG#v}"
  mkdir -p "$TMP/flash/$V" "$DL/$V"
  HAS_XL=0
  gh release view "$TAG" -R "$REPO" --json assets --jq '.assets[].name' | grep -qx firmware-xl.bin && HAS_XL=1
  for PAIR in firmware.bin:manifest.json firmware-xl.bin:manifest-xl.json; do
    BIN="${PAIR%%:*}" MAN="${PAIR#*:}"
    [ "$BIN" = firmware-xl.bin ] && [ "$HAS_XL" = 0 ] && continue
    if [ -f "$TMP/flash/$V/$BIN" ] && [ -f "$TMP/flash/$V/$MAN" ]; then
      echo "== $TAG $BIN is already there"
      continue
    fi
    echo "== downloading $TAG $BIN"
    gh release download "$TAG" -R "$REPO" -D "$DL/$V" -p "$BIN" -p "$BIN.sha256"
    (cd "$DL/$V" && sha256sum -c "$BIN.sha256" >/dev/null) || { echo "Checksum of $TAG $BIN does not match." >&2; exit 1; }
    grep -aq "$V" "$DL/$V/$BIN" || { echo "Version $V not found in $BIN of $TAG." >&2; exit 1; }
    cp "$DL/$V/$BIN" "$TMP/flash/$V/$BIN"
    manifest "$TMP/flash/$V" "$V" "$BIN" "$MAN"
  done
done

# List for the page (newest first; the description comes from CHANGELOG.md: first paragraph under the heading)
python3 - "$DL/releases.json" "$ROOT/CHANGELOG.md" "$TMP/flash/versions.json" "$TMP/flash" <<'PY'
import json, os, re, sys
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
        elif par:  # the first paragraph (up to the first blank line) is the summary
            notes[cur] = " ".join(par)
if cur and cur not in notes and par:
    notes[cur] = " ".join(par)
def boards(v):  # which boards this version exists for (the XL from the first release with firmware-xl.bin)
    return [b for b, f in (("original", "firmware.bin"), ("xl", "firmware-xl.bin")) if os.path.exists(os.path.join(sys.argv[4], v, f))]
out = [{"version": r["tagName"][1:], "date": r["publishedAt"][:10], "note": notes.get(r["tagName"][1:], ""),
        "boards": boards(r["tagName"][1:])} for r in rel]
json.dump({"versions": out}, open(sys.argv[3], "w"), ensure_ascii=False, indent=1)
PY

cp firmware/web/flash/index.html "$TMP/flash/index.html"
cp "$BUILD/bootloader.bin" "$BUILD/partitions.bin" "$TMP/flash/"
cp "$BOOT_APP0" "$TMP/flash/boot_app0.bin"
# Clean up old loose files from the first setup (latest version only)
rm -f "$TMP/flash/firmware.bin" "$TMP/flash/manifest.json"
touch "$TMP/.nojekyll"
# Dry run: DRYRUN=<dir> puts the result in that directory instead of publishing (for local testing)
if [ -n "${DRYRUN:-}" ]; then
  mkdir -p "$DRYRUN" && cp -r "$TMP/flash" "$DRYRUN/" && cp firmware/web/icon.svg firmware/web/*.png "$DRYRUN/"
  echo "Dry run done in $DRYRUN"
  exit 0
fi
git -C "$TMP" add -A flash .nojekyll
if git -C "$TMP" diff --cached --quiet; then
  echo "Install page is already up to date."
  exit 0
fi
git -C "$TMP" commit -q -m "Install page updated (latest: $(echo $TAGS | cut -d' ' -f1))"
git -C "$TMP" push -q origin gh-pages
echo "Published: https://newmedia.github.io/deskbuddy/flash/ (may take a minute)"
