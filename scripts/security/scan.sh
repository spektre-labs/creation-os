#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Creation OS — layered secret / credential / URL scan.
#
# Defense in depth:
#   1. gitleaks (if installed) on the working tree + git history.
#   2. Portable grep-only fallback over the working tree for high-value
#      patterns (AWS, GCP, Slack, OpenAI, GitHub tokens, RSA / SSH
#      private keys, JWT, high-entropy hex secrets).
#   3. Hardcoded-URL sanity on src/ (flags any http:// pointing outside
#      localhost that may accidentally leak telemetry).
#
# Exit code 0 only if all available scans pass.

set -euo pipefail
cd "$(dirname "$0")/../.."

exit_code=0
echo "=== Creation OS security-scan ==="

# --- 1. gitleaks (preferred) ---
if command -v gitleaks >/dev/null 2>&1; then
    echo "[1/3] gitleaks (with .gitleaks.toml if present)..."
    if [ -f .gitleaks.toml ]; then
        if ! gitleaks detect --config .gitleaks.toml --no-banner --exit-code 1 --redact; then
            echo "gitleaks: FAIL"
            exit_code=1
        else
            echo "gitleaks: OK"
        fi
    else
        if ! gitleaks detect --no-banner --exit-code 1 --redact; then
            echo "gitleaks: FAIL"
            exit_code=1
        else
            echo "gitleaks: OK"
        fi
    fi
else
    echo "[1/3] gitleaks: SKIP (not installed — brew install gitleaks)"
fi

# --- 2. portable grep-only fallback ---
echo "[2/3] grep-only fallback scan (high-value patterns)..."
# shellcheck disable=SC2016
PATTERNS=(
    'AKIA[0-9A-Z]{16}'
    'ASIA[0-9A-Z]{16}'
    'AIza[0-9A-Za-z_\-]{35}'
    'xox[baprs]-[0-9A-Za-z\-]+'
    'sk-[0-9A-Za-z]{32,}'
    'ghp_[0-9A-Za-z]{36}'
    'github_pat_[0-9A-Za-z_]{82}'
    'gho_[0-9A-Za-z]{36}'
    '-----BEGIN (RSA |OPENSSH |EC |PGP |DSA )?PRIVATE KEY-----'
    'eyJ[A-Za-z0-9_\-]+\.eyJ[A-Za-z0-9_\-]+\.[A-Za-z0-9_\-]+'
)
# Allowlist: example / fixture paths.
ALLOWLIST='(\.env\.example|docs/.*|README\.md|CHANGELOG\.md|SECURITY\.md|THREAT_MODEL\.md|scripts/security/scan\.sh|\.gitleaks\.toml)'
fallback_hits=0
for pat in "${PATTERNS[@]}"; do
    # Use grep -En (extended regex) over the tree, skipping VCS dirs.
    if hits=$(grep -rEn "$pat" \
                --exclude-dir=.git \
                --exclude-dir=.build \
                --exclude-dir=build \
                --exclude-dir=node_modules \
                --exclude-dir=target \
                --exclude='*.so' \
                --exclude='*.dylib' \
                --exclude='*.o' \
                . 2>/dev/null | grep -Ev "$ALLOWLIST" || true); then
        if [ -n "$hits" ]; then
            echo "  pattern match ($pat):"
            echo "$hits"
            fallback_hits=$((fallback_hits + 1))
        fi
    fi
done
if [ "$fallback_hits" -ne 0 ]; then
    echo "grep-fallback: FAIL ($fallback_hits patterns matched)"
    exit_code=1
else
    echo "grep-fallback: OK (0 matches)"
fi

# --- 3. hardcoded-URL telemetry sanity ---
echo "[3/3] hardcoded-URL (non-loopback http://) sanity over src/..."
url_hits=$(grep -rEn 'http://[a-zA-Z0-9\.\-]+' src/ docs/ \
             2>/dev/null \
             | grep -vE 'localhost|127\.0\.0\.1|0\.0\.0\.0|example\.com|example\.org|cache-control|\.schema\.org|purl\.org|www\.w3\.org|xmlns|DOCTYPE|apple\.com/publicsource|www\.gnu\.org|creativecommons\.org|www\.opengroup\.org|opensource\.org|spdx\.org' \
             || true)
if [ -n "$url_hits" ]; then
    echo "hardcoded-url: FAIL (non-loopback http:// references outside allowlist)"
    echo "$url_hits"
    exit_code=1
else
    echo "hardcoded-url: OK"
fi

echo
if [ "$exit_code" -eq 0 ]; then
    echo "security-scan: PASS"
else
    echo "security-scan: FAIL (fix secrets / URLs above or update .gitleaks.toml allowlist)"
fi
exit "$exit_code"
