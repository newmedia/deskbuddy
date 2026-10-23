#!/usr/bin/env bash
# Publishes the PWA (firmware/web) to the gh-pages branch, without GitHub Actions.
# GitHub Pages serves that branch at https://newmedia.github.io/deskbuddy/
# (one-time setup: repo -> Settings -> Pages -> Source: "Deploy from a branch", branch gh-pages, folder / (root)).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
FILES=(index.html manifest.webmanifest sw.js icon.svg icon-192.png icon-512.png icon-maskable-512.png apple-touch-icon.png)
SRC_REV="$(git rev-parse --short HEAD)"
TMP="$(mktemp -d)"
trap 'git worktree remove --force "$TMP" >/dev/null 2>&1 || true; rm -rf "$TMP"' EXIT

git fetch -q origin gh-pages 2>/dev/null || true
if git rev-parse -q --verify origin/gh-pages >/dev/null; then
  git worktree add -q -B gh-pages "$TMP" origin/gh-pages
else
  git worktree add -q --detach "$TMP"
  git -C "$TMP" checkout -q --orphan gh-pages
  git -C "$TMP" rm -rq --cached . >/dev/null 2>&1 || true
  find "$TMP" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
fi

for f in "${FILES[@]}"; do cp "firmware/web/$f" "$TMP/$f"; done
# Install page (flash/): only the page itself; the firmware comes from tools/publish-flash.sh
mkdir -p "$TMP/flash"
cp firmware/web/flash/index.html "$TMP/flash/index.html"
touch "$TMP/.nojekyll"  # serve files unchanged
git -C "$TMP" add -A "${FILES[@]}" flash/index.html .nojekyll
if git -C "$TMP" diff --cached --quiet; then
  echo "PWA is already up to date."
  exit 0
fi
git -C "$TMP" commit -q -m "PWA updated from main@$SRC_REV"
git -C "$TMP" push -q origin gh-pages
echo "Published: https://newmedia.github.io/deskbuddy/ (may take a minute)"
