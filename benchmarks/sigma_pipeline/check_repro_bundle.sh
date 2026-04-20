#!/usr/bin/env bash
#
# Smoke test for FIX-4: automatic reproduction bundle.
#
# Invokes scripts/repro_bundle.sh in --quick mode (no make runs so
# CI is O(ms) instead of O(90s)) and asserts the bundle contains
# every mandatory field listed in docs/REPRO_BUNDLE_TEMPLATE.md.
# Also exercises the --json path.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ---- 1. text form ---------------------------------------------------------
bash scripts/repro_bundle.sh --quick --out "$TMP/bundle.txt"

grep -q "^=== CREATION OS · REPRO BUNDLE ===" "$TMP/bundle.txt" \
    || { echo "FAIL missing header" >&2; exit 1; }
for field in date_utc hostname uname cpu os cc git_sha git_branch git_dirty_files merge_gate; do
    grep -qE "^$field" "$TMP/bundle.txt" \
        || { echo "FAIL missing field: $field" >&2; exit 2; }
done
grep -q "check-sigma-pipeline" "$TMP/bundle.txt" \
    || { echo "FAIL missing check-sigma-pipeline line" >&2; exit 3; }
grep -q "^=== END ===" "$TMP/bundle.txt" \
    || { echo "FAIL missing footer" >&2; exit 4; }

# ---- 2. ISO-8601 date -----------------------------------------------------
grep -qE '^date_utc *: [0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z' "$TMP/bundle.txt" \
    || { echo "FAIL date_utc not ISO-8601" >&2; exit 5; }

# ---- 3. git SHA is 40-hex -------------------------------------------------
grep -qE '^git_sha *: [0-9a-f]{40}$' "$TMP/bundle.txt" \
    || { echo "FAIL git_sha not 40-hex" >&2; exit 6; }

# ---- 4. JSON form ---------------------------------------------------------
bash scripts/repro_bundle.sh --quick --json --out "$TMP/bundle.json"
for key in '"tool"' '"date_utc"' '"hostname"' '"uname"' '"cpu"' '"os"' '"cc"' \
           '"git_sha"' '"git_branch"' '"git_dirty_files"' '"merge_gate"' \
           '"check-sigma-pipeline"'; do
    grep -q "$key" "$TMP/bundle.json" \
        || { echo "FAIL JSON bundle missing $key" >&2; exit 7; }
done
grep -q '"tool":"cos repro_bundle"' "$TMP/bundle.json" \
    || { echo "FAIL JSON tool field" >&2; exit 8; }

echo "check-repro-bundle: PASS (text + json bundles, all mandatory fields)"
