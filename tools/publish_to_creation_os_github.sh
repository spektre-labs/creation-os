#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Commercial:    licensing@spektre-labs.com
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
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

make merge-gate && make stack-ultimate && make rust-iron-lint

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

# Never ship macOS Finder duplicate paths ("name 2.ext" / "name 2/").
# rsync --delete alone does not remove receiver-only junk reliably with excludes;
# prune explicitly in the staging clone (not in the working tree).
find "$STAGE/repo" -depth \( -name '* 2.*' -o -name '* 2' \) ! -path '*/.git/*' -print0 2>/dev/null | xargs -0 rm -rf || true

cd "$STAGE/repo"

# Never ship local Makefile binaries (rsync ignores .git only; artifacts may exist on disk).
for bin in creation_os creation_os_v6 creation_os_v7 creation_os_v9 creation_os_v10 creation_os_v11 creation_os_v12 creation_os_v15 creation_os_v16 creation_os_v20 creation_os_v21 creation_os_v22 creation_os_v23 creation_os_v24 creation_os_v25 creation_os_v26 test_bsc gemm_vs_bsc coherence_gate_batch hv_agi_gate_neon oracle_speaks oracle_ultimate genesis qhdc; do
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
