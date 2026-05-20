#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST_PATH="$ROOT_DIR/custom_components/lumentreelocal/manifest.json"
CHANGELOG_PATH="$ROOT_DIR/CHANGELOG.md"
REPO_SLUG="nlkcodenew/lumentreelocal"

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "Missing required command: $1" >&2
    exit 1
  }
}

require_clean_worktree() {
  if [[ -n "$(git -C "$ROOT_DIR" status --short)" ]]; then
    echo "Worktree is not clean. Commit or stash changes before releasing." >&2
    exit 1
  fi
}

extract_version() {
  python3 - <<'PY' "$MANIFEST_PATH"
import json, sys
from pathlib import Path
manifest = json.loads(Path(sys.argv[1]).read_text())
print(manifest["version"])
PY
}

extract_release_notes() {
  python3 - <<'PY' "$CHANGELOG_PATH" "$1"
import sys
from pathlib import Path

changelog_path = Path(sys.argv[1])
version = sys.argv[2]
text = changelog_path.read_text()
header_prefix = f"## [{version}] - "
start = text.find(header_prefix)
if start == -1:
    raise SystemExit(f"Missing changelog section for version {version}")
end = text.find("\n## [", start + 1)
section = text[start:end] if end != -1 else text[start:]
print(section.strip())
PY
}

require_cmd git
require_cmd python3
require_cmd gh

require_clean_worktree

VERSION="$(extract_version)"
TAG="v$VERSION"
NOTES_FILE="/tmp/lumentree-${VERSION}-release-notes.md"

if git -C "$ROOT_DIR" rev-parse "$TAG" >/dev/null 2>&1; then
  echo "Tag $TAG already exists locally." >&2
  exit 1
fi

if git -C "$ROOT_DIR" ls-remote --tags origin "$TAG" | grep -q "$TAG"; then
  echo "Tag $TAG already exists on origin." >&2
  exit 1
fi

if gh release view "$TAG" --repo "$REPO_SLUG" >/dev/null 2>&1; then
  echo "GitHub Release $TAG already exists." >&2
  exit 1
fi

extract_release_notes "$VERSION" > "$NOTES_FILE"

git -C "$ROOT_DIR" push
git -C "$ROOT_DIR" tag "$TAG"
git -C "$ROOT_DIR" push origin "$TAG"

gh release create "$TAG" \
  --repo "$REPO_SLUG" \
  --title "$TAG" \
  --notes-file "$NOTES_FILE"

echo "Released $TAG"
