#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
#
# tools/license/check_headers.sh
#
# CI-friendly verifier: ensures every source file in scope carries
# the canonical SCSL-1.0 / AGPL-3.0 dual SPDX header, and that
# LICENSE.sha256 matches the current LICENSE-SCSL-1.0.md.
#
# Exit non-zero on any failure. Designed to be wired into:
#   - .pre-commit-config.yaml  (pre-push hook)
#   - .github/workflows/security.yml  (CI gate)
#   - Makefile  (`make license-check`)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

EXIT=0

echo "── 1/3  License documents present"
for f in LICENSE LICENSE-SCSL-1.0.md LICENSE-AGPL-3.0.txt LICENSE.sha256 NOTICE COMMERCIAL_LICENSE.md CLA.md docs/LICENSING.md docs/LICENSE_MATRIX.md; do
    if [ -f "${f}" ]; then
        printf '   ok   %s\n' "${f}"
    else
        printf '   MISS %s\n' "${f}"
        EXIT=1
    fi
done

echo "── 2/3  License-Bound Receipt anchor (SCSL §11)"
if bash tools/license/license_sha256.sh --check; then
    :
else
    EXIT=1
fi

echo "── 3/3  SPDX headers across source tree"
if bash tools/license/apply_headers.sh --check; then
    :
else
    EXIT=1
fi

if [ "${EXIT}" -eq 0 ]; then
    echo "license-check: ALL OK"
else
    echo "license-check: FAILED — see lines above" >&2
fi

exit "${EXIT}"
