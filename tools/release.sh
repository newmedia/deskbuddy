#!/usr/bin/env bash
# Creates a firmware release on GitHub: builds locally (env esp32) with the version from the tag and
# uploads firmware.bin + firmware.bin.sha256 as assets. The deskbuddy fetches those itself.
#
#   tools/release.sh 0.2.0          (or v0.2.0)
#
# Requirements: clean working tree on main, gh logged in, and a section "## v0.2.0" in CHANGELOG.md
# describing what is new; that text becomes the release description.
# Existing releases are never modified or deleted: old versions stay available.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="newmedia/deskbuddy"
cd "$ROOT"

[ $# -ge 1 ] || { echo "usage: tools/release.sh <version>   e.g. tools/release.sh 0.2.0" >&2; exit 1; }
VERSION="${1#v}"
TAG="v$VERSION"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Version must be x.y.z (semver), not '$1'." >&2; exit 1; }

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

# Newer than the latest release? (sort -V understands semver ordering)
LAST="$(git tag -l 'v*' --sort=-v:refname | grep -vx "$TAG" | head -1 || true)"
if [ -n "$LAST" ] && [ "$(printf '%s\n%s\n' "${LAST#v}" "$VERSION" | sort -V | tail -1)" != "$VERSION" ]; then
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

echo "== building $TAG"
cd firmware
PLATFORMIO_BUILD_FLAGS="-D DESKBUDDY_VERSION=\\\"$VERSION\\\"" ../.venv/bin/pio run -e esp32
cd "$ROOT"

OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
cp firmware/.pio/build/esp32/firmware.bin "$OUT/firmware.bin"
grep -aq "$VERSION" "$OUT/firmware.bin" || { echo "Version $VERSION not found in the binary; the build flag did not work." >&2; exit 1; }
(cd "$OUT" && sha256sum firmware.bin > firmware.bin.sha256)
echo "$NOTES" > "$OUT/notes.md"
echo "== $(cat "$OUT/firmware.bin.sha256")"

echo "== tagging and uploading"
if [ "$RESUME" = 0 ]; then
  git tag -a "$TAG" -m "deskbuddy $TAG"
fi
git push origin "$TAG"
gh release create "$TAG" -R "$REPO" --verify-tag --title "deskbuddy $TAG" --notes-file "$OUT/notes.md" \
  "$OUT/firmware.bin" "$OUT/firmware.bin.sha256"
echo "== done: https://github.com/$REPO/releases/tag/$TAG"
