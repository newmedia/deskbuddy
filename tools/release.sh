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
[ "$(gh api "repos/$REPO" --jq .permissions.push 2>/dev/null)" = true ] || {
  echo "Het gh-token mag geen releases maken in $REPO (geen schrijfrechten via de API)." >&2
  echo "Log in met een account/token dat dat wel mag (zie docs/firmware.md)." >&2
  exit 1
}
[ -z "$(git status --porcelain)" ] || { echo "Werkmap is niet schoon; commit eerst." >&2; exit 1; }

if gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
  echo "Release $TAG bestaat al. Kies een nieuwe versie; bestaande releases blijven zoals ze zijn." >&2
  exit 1
fi
# Tag bestaat al zonder release (bijv. een eerdere poging faalde bij het uploaden): alleen hervatten
# als hij op de huidige commit staat.
RESUME=0
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
  [ "$(git rev-list -n1 "$TAG")" = "$(git rev-parse HEAD)" ] || {
    echo "Tag $TAG bestaat al op een andere commit. Kies een nieuwe versie." >&2
    exit 1
  }
  RESUME=1
  echo "== tag $TAG bestaat al op HEAD zonder release: hervatten"
fi

# Nieuwer dan de laatste release? (sort -V kent semver-volgorde)
LAST="$(git tag -l 'v*' --sort=-v:refname | grep -vx "$TAG" | head -1 || true)"
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
if [ "$RESUME" = 0 ]; then
  git tag -a "$TAG" -m "deskbuddy $TAG"
fi
git push origin "$TAG"
gh release create "$TAG" -R "$REPO" --verify-tag --title "deskbuddy $TAG" --notes-file "$OUT/notes.md" \
  "$OUT/firmware.bin" "$OUT/firmware.bin.sha256"
echo "== klaar: https://github.com/$REPO/releases/tag/$TAG"
