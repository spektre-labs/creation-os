#!/usr/bin/env bash
# Publish this portable Creation OS directory to https://github.com/spektre-labs/creation-os
# Requires working GitHub auth (SSH key, or HTTPS + credential, or: gh auth login).
#
# CANONICAL REPO ONLY: spektre-labs/creation-os. Do not point a parent monorepo's git remote
# here; see docs/CANONICAL_GIT_REPOSITORY.md.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -f creation_os_v2.c ]]; then
  echo "error: creation_os_v2.c missing — run from the portable Creation OS tree root." >&2
  exit 1
fi

make check && make check-v6 && make check-v7 && make check-v9 && make check-v10 && make check-v11

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

# Exclude macOS-style duplicate paths ("filename 2.ext") — never ship accidental copies.
RSYNC_EXCLUDES=(
  --exclude='.git'
  --exclude='* 2.md' --exclude='* 2.c' --exclude='* 2.h' --exclude='* 2.sh'
  --exclude='* 2.yml' --exclude='* 2.txt' --exclude='* 2.cpp' --exclude='* 2.cff'
  --exclude='* 2.bib' --exclude='Makefile 2' --exclude='.gitignore 2' --exclude='LICENSE 2'
  --exclude='* 2/' --exclude='CITATION 2.cff'
)
rsync -a --delete "${RSYNC_EXCLUDES[@]}" "$ROOT/" "$STAGE/repo/"
cd "$STAGE/repo"

# Never ship local Makefile binaries (rsync ignores .git only; artifacts may exist on disk).
for bin in creation_os creation_os_v6 creation_os_v7 creation_os_v9 creation_os_v10 creation_os_v11 test_bsc gemm_vs_bsc coherence_gate_batch hv_agi_gate_neon oracle_speaks oracle_ultimate genesis qhdc; do
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
