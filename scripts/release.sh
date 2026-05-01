#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Maintainer: tag a release on the canonical creation-os tree after gates PASS.
# Usage: scripts/release.sh v3.3.0
# Optional: CREATION_OS_RELEASE_SKIP_MERGE_GATE=1  (emergency only; not for CI)
set -euo pipefail

VERSION="${1:?Usage: $0 VERSION   (example: $0 v3.3.0)}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "=== Releasing Creation OS $VERSION ==="

if [[ "${CREATION_OS_RELEASE_SKIP_MERGE_GATE:-}" == "1" ]]; then
    echo "WARNING: CREATION_OS_RELEASE_SKIP_MERGE_GATE=1 — merge-gate skipped." >&2
else
    make check-agi
    make merge-gate
fi

./cos demo --batch

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "ERROR: working tree or index is not clean; commit or stash before tagging." >&2
    exit 1
fi

if git rev-parse "refs/tags/$VERSION" >/dev/null 2>&1; then
    echo "ERROR: tag $VERSION already exists." >&2
    exit 1
fi

git tag -a "$VERSION" -m "$VERSION release"

echo "=== Created annotated tag $VERSION (local) ==="
echo "Push tag when ready:"
echo "  git push origin $VERSION"
echo "Create GitHub release:"
echo "  https://github.com/spektre-labs/creation-os/releases/new?tag=$VERSION"
echo ""
echo "After the tarball exists, update packaging/homebrew-cos/Formula/creation-os.rb"
echo "url/sha256 (brew fetch --build-from-source creation-os)."
