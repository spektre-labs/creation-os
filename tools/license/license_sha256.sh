#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
#
# tools/license/license_sha256.sh
#
# Recompute the canonical SHA-256 of LICENSE-SCSL-1.0.md and either
# (a) print it (default), (b) write it to LICENSE.sha256 with --pin,
# or (c) verify LICENSE.sha256 matches the current LICENSE with --check.
#
# This script is the reference implementation for SCSL-1.0 §11
# (Cryptographic License-Bound Receipt). The hash it computes is
# the value that MUST appear in the `license_sha256` field of every
# Receipt emitted by Creation OS.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LICENSE_FILE="${REPO_ROOT}/LICENSE-SCSL-1.0.md"
PIN_FILE="${REPO_ROOT}/LICENSE.sha256"

if [ ! -f "${LICENSE_FILE}" ]; then
    echo "license_sha256: ERROR: ${LICENSE_FILE} not found" >&2
    exit 2
fi

# Pick a SHA-256 backend (POSIX sha256sum or macOS shasum).
hash_file() {
    local f="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$f" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$f" | awk '{print $1}'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha256 "$f" | awk '{print $NF}'
    else
        echo "license_sha256: ERROR: no sha256 backend (need sha256sum / shasum / openssl)" >&2
        exit 3
    fi
}

CURRENT="$(hash_file "${LICENSE_FILE}")"
ACTION="${1:---print}"

case "${ACTION}" in
    --print|-p|"")
        echo "${CURRENT}"
        ;;
    --pin)
        printf '%s  LICENSE-SCSL-1.0.md\n' "${CURRENT}" > "${PIN_FILE}"
        echo "license_sha256: pinned ${CURRENT} → ${PIN_FILE}"
        ;;
    --check|-c)
        if [ ! -f "${PIN_FILE}" ]; then
            echo "license_sha256: ERROR: ${PIN_FILE} not present; run --pin first" >&2
            exit 4
        fi
        PINNED="$(awk '{print $1}' "${PIN_FILE}")"
        if [ "${CURRENT}" = "${PINNED}" ]; then
            echo "license_sha256: OK (${CURRENT})"
        else
            echo "license_sha256: MISMATCH" >&2
            echo "  pinned : ${PINNED}" >&2
            echo "  current: ${CURRENT}" >&2
            exit 1
        fi
        ;;
    --help|-h)
        cat <<'EOF'
usage: tools/license/license_sha256.sh [--print|--pin|--check|--help]

  --print  (default)  print SHA-256 of LICENSE-SCSL-1.0.md to stdout
  --pin               write that SHA-256 to LICENSE.sha256
  --check             verify LICENSE.sha256 matches the current file

This is the reference implementation for the License-Bound Receipt
(SCSL-1.0 §11). Every Receipt emitted by Creation OS MUST carry the
hash this script prints.
EOF
        ;;
    *)
        echo "license_sha256: unknown action: ${ACTION}" >&2
        echo "see --help" >&2
        exit 2
        ;;
esac
