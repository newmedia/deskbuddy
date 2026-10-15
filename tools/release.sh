#!/usr/bin/env bash
# Maakt een firmware-release op GitHub: bouwt lokaal (env esp32) met de versie uit de tag en
# uploadt firmware.bin + firmware.bin.sha256 als assets. De deskbuddy haalt die zelf op.
#
#   tools/release.sh 0.2.0          (of v0.2.0)
#
# Vereisten: schone werkmap op main, gh ingelogd, en in CHANGELOG.md een sectie "## v0.2.0"
# met wat er nieuw is; die tekst wordt de omschrijving van de release.
# Bestaande releases worden nooit aangepast of verwijderd: oude versies blijven beschikbaar.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="newmedia/deskbuddy"
cd "$ROOT"

[ $# -ge 1 ] || { echo "gebruik: tools/release.sh <versie>   bijv. tools/release.sh 0.2.0" >&2; exit 1; }
VERSION="${1#v}"
TAG="v$VERSION"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Versie moet x.y.z zijn (semver), niet '$1'." >&2; exit 1; }

command -v gh >/dev/null || { echo "gh (GitHub CLI) ontbreekt." >&2; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "Eerst inloggen: gh auth login" >&2; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo "Werkmap is niet schoon; commit eerst." >&2; exit 1; }

if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null || gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
  echo "$TAG bestaat al. Kies een nieuwe versie; bestaande releases blijven zoals ze zijn." >&2
  exit 1
fi

# Nieuwer dan de laatste release? (sort -V kent semver-volgorde)
LAST="$(git tag -l 'v*' --sort=-v:refname | head -1)"
if [ -n "$LAST" ] && [ "$(printf '%s\n%s\n' "${LAST#v}" "$VERSION" | sort -V | tail -1)" != "$VERSION" ]; then
  echo "$TAG is niet nieuwer dan $LAST; apparaten zouden hem negeren." >&2
  exit 1
fi

# Release-notes uit CHANGELOG.md: alles tussen "## vX.Y.Z" en de volgende "## ".
NOTES="$(awk -v t="## $TAG" '$0 ~ "^## " { if (on) exit; on = ($1" "$2 == t) ; next } on' CHANGELOG.md 2>/dev/null || true)"
if [ -z "$(echo "$NOTES" | tr -d '[:space:]')" ]; then
  echo "Geen omschrijving gevonden voor $TAG in CHANGELOG.md." >&2
  echo "Voeg bovenaan een sectie toe, commit die, en probeer opnieuw. Concept op basis van de commits:" >&2
  echo >&2
  echo "## $TAG" >&2
  git log ${LAST:+$LAST..}HEAD --no-merges --pretty='- %s' >&2
  exit 1
fi

echo "== bouwen $TAG"
cd firmware
PLATFORMIO_BUILD_FLAGS="-D DESKBUDDY_VERSION=\\\"$VERSION\\\"" ../.venv/bin/pio run -e esp32
cd "$ROOT"

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
cp firmware/.pio/build/esp32/firmware.bin "$OUT/firmware.bin"
grep -aq "$VERSION" "$OUT/firmware.bin" || { echo "Versie $VERSION niet in de binary gevonden; build-flag werkte niet." >&2; exit 1; }
(cd "$OUT" && sha256sum firmware.bin > firmware.bin.sha256)
echo "$NOTES" > "$OUT/notes.md"
echo "== $(cat "$OUT/firmware.bin.sha256")"

echo "== taggen en uploaden"
git tag -a "$TAG" -m "deskbuddy $TAG"
git push origin "$TAG"
gh release create "$TAG" -R "$REPO" --verify-tag --title "deskbuddy $TAG" --notes-file "$OUT/notes.md" \
  "$OUT/firmware.bin" "$OUT/firmware.bin.sha256"
echo "== klaar: https://github.com/$REPO/releases/tag/$TAG"
