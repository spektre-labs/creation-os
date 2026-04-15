#!/usr/bin/env bash
# Publish this portable Creation OS directory to https://github.com/spektre-labs/creation-os
# Requires working GitHub auth (SSH key, or HTTPS + credential, or: gh auth login).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -f creation_os_v2.c ]]; then
  echo "error: creation_os_v2.c missing — run from the portable Creation OS tree root." >&2
  exit 1
fi

make check

REMOTE="${CREATION_OS_REMOTE:-https://github.com/spektre-labs/creation-os.git}"
BRANCH="${CREATION_OS_BRANCH:-main}"
MSG="${CREATION_OS_COMMIT_MSG:-Publish portable Creation OS tree}"

STAGE="$(mktemp -d)"
cleanup() { rm -rf "$STAGE"; }
trap cleanup EXIT

if ! git clone --branch "$BRANCH" "$REMOTE" "$STAGE/repo" 2>/dev/null; then
  git clone "$REMOTE" "$STAGE/repo"
  git -C "$STAGE/repo" checkout "$BRANCH"
fi

rsync -a --delete --exclude '.git' "$ROOT/" "$STAGE/repo/"
cd "$STAGE/repo"

# Never ship local Makefile binaries (rsync ignores .git only; artifacts may exist on disk).
for bin in creation_os test_bsc gemm_vs_bsc coherence_gate_batch oracle_speaks oracle_ultimate genesis qhdc; do
  rm -f "$bin"
done
rm -rf .build

if git diff --quiet && git diff --cached --quiet; then
  echo "No file changes vs remote — nothing to commit."
  exit 0
fi

git add -A
git commit -m "$MSG"
echo "Pushing to $REMOTE ($BRANCH) …"
git push origin "$BRANCH"
echo "Done."
