#!/usr/bin/env bash
# Creates a firmware release on GitHub: builds locally with the version from the tag, for both boards,
# and uploads the assets. Each deskbuddy fetches its own: the Original firmware.bin (env esp32), the XL
# firmware-xl.bin (env esp32-xl), each with a .sha256.
#
#   tools/release.sh 0.2.0                  (or v0.2.0)
#   tools/release.sh 0.2.0 --original-only  without firmware-xl.bin (e.g. while the XL is untested)
#   tools/release.sh 1.4.0-beta.1           a test version (also -alpha.N, -rc.N): a GitHub pre-release.
#                                           Buddies never install it by themselves (they follow releases/latest);
#                                           testers install it via the install page ("test versions") or by hand.
#
# Requirements: clean working tree on main, gh logged in, and a section "## v0.2.0" in CHANGELOG.md
# describing what is new; that text becomes the release description.
# Existing releases are never modified or deleted: old versions stay available.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="newmedia/deskbuddy"
cd "$ROOT"

[ $# -ge 1 ] || { echo "usage: tools/release.sh <version> [--original-only]   e.g. tools/release.sh 0.2.0" >&2; exit 1; }
VERSION="${1#v}"
BUILDS="esp32:firmware.bin esp32-xl:firmware-xl.bin"
[ "${2:-}" = "--original-only" ] && BUILDS="esp32:firmware.bin"
TAG="v$VERSION"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+(-(alpha|beta|rc)\.[0-9]+)?$ ]] ||
  { echo "Version must be x.y.z or x.y.z-beta.N (also alpha/rc), not '$1'." >&2; exit 1; }
PRE=0
[[ "$VERSION" == *-* ]] && PRE=1

command -v gh >/dev/null || { echo "gh (GitHub CLI) is missing." >&2; exit 1; }
gh auth status >/dev/null 2>&1 || { echo "Log in first: gh auth login" >&2; exit 1; }
[ "$(gh api "repos/$REPO" --jq .permissions.push 2>/dev/null)" = true ] || {
  echo "The gh token is not allowed to create releases in $REPO (no write access via the API)." >&2
  echo "Log in with an account/token that is allowed to (see docs/firmware.md)." >&2
  exit 1
}
[ -z "$(git status --porcelain)" ] || { echo "Working tree is not clean; commit first." >&2; exit 1; }

if gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
  echo "Release $TAG already exists. Choose a new version; existing releases are left as they are." >&2
  exit 1
fi
# Tag already exists without a release (e.g. an earlier attempt failed during upload): only resume
# if it points at the current commit.
RESUME=0
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
  [ "$(git rev-list -n1 "$TAG")" = "$(git rev-parse HEAD)" ] || {
    echo "Tag $TAG already exists on a different commit. Choose a new version." >&2
    exit 1
  }
  RESUME=1
  echo "== tag $TAG already exists on HEAD without a release: resuming"
fi

# Newer than the latest final release? Pre-release tags do not count (a final 1.4.0 must stay possible
# after 1.4.0-beta.1), and a pre-release is compared by its numbers (1.4.0-beta.1 needs 1.4.0 > last).
LAST="$(git tag -l 'v*' --sort=-v:refname | grep -v -- '-' | grep -vx "$TAG" | head -1 || true)"
NUM="${VERSION%%-*}"
if [ -n "$LAST" ] && { [ "${LAST#v}" = "$NUM" ] || [ "$(printf '%s\n%s\n' "${LAST#v}" "$NUM" | sort -V | tail -1)" != "$NUM" ]; }; then
  echo "$TAG is not newer than $LAST; devices would ignore it." >&2
  exit 1
fi

# Release notes from CHANGELOG.md: everything between "## vX.Y.Z" and the next "## ".
NOTES="$(awk -v t="## $TAG" '$0 ~ "^## " { if (on) exit; on = ($1" "$2 == t) ; next } on' CHANGELOG.md 2>/dev/null || true)"
if [ -z "$(echo "$NOTES" | tr -d '[:space:]')" ]; then
  echo "No description found for $TAG in CHANGELOG.md." >&2
  echo "Add a section at the top, commit it, and try again. Draft based on the commits:" >&2
  echo >&2
  echo "## $TAG" >&2
  git log ${LAST:+$LAST..}HEAD --no-merges --pretty='- %s' >&2
  exit 1
fi

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
# env -> asset name
ASSETS=()
for PAIR in $BUILDS; do
  ENV="${PAIR%%:*}" BIN="${PAIR#*:}"
  echo "== building $TAG for $ENV"
  (cd firmware && PLATFORMIO_BUILD_FLAGS="-D DESKBUDDY_VERSION=\\\"$VERSION\\\"" ../.venv/bin/pio run -e "$ENV")
  cp "firmware/.pio/build/$ENV/firmware.bin" "$OUT/$BIN"
  grep -aq "$VERSION" "$OUT/$BIN" || { echo "Version $VERSION not found in $BIN; the build flag did not work." >&2; exit 1; }
  (cd "$OUT" && sha256sum "$BIN" > "$BIN.sha256")
  echo "== $(cat "$OUT/$BIN.sha256")"
  ASSETS+=("$OUT/$BIN" "$OUT/$BIN.sha256")
done
echo "$NOTES" > "$OUT/notes.md"

echo "== tagging and uploading"
if [ "$RESUME" = 0 ]; then
  git tag -a "$TAG" -m "deskbuddy $TAG"
fi
git push origin "$TAG"
PREFLAG=()
[ "$PRE" = 1 ] && PREFLAG=(--prerelease)  # not "latest": buddies do not see it
gh release create "$TAG" -R "$REPO" --verify-tag --title "deskbuddy $TAG" --notes-file "$OUT/notes.md" \
  ${PREFLAG[@]+"${PREFLAG[@]}"} "${ASSETS[@]}"
echo "== done: https://github.com/$REPO/releases/tag/$TAG"
