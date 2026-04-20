#!/usr/bin/env bash
# Smoke test for the σ-reinforce primitive binary.
#
# Builds nothing (the Makefile target is the dependency); runs the
# binary and greps the JSON summary for:
#   - "self_test_rc":0
#   - "pass":true
#   - "max_rounds":3
#   - action enum labels present
set -euo pipefail

cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_reinforce
if [[ ! -x "$BIN" ]]; then
    echo "check-sigma-reinforce: binary missing: $BIN" >&2
    exit 2
fi

OUT="$("$BIN")"
echo "  · summary: $OUT"

if ! grep -q '"self_test_rc":0' <<<"$OUT"; then
    echo "check-sigma-reinforce: self_test_rc != 0" >&2
    exit 3
fi
if ! grep -q '"pass":true' <<<"$OUT"; then
    echo "check-sigma-reinforce: pass != true" >&2
    exit 4
fi
if ! grep -q '"max_rounds":3' <<<"$OUT"; then
    echo "check-sigma-reinforce: max_rounds != 3" >&2
    exit 5
fi
if ! grep -q 'ACCEPT' <<<"$OUT" \
   || ! grep -q 'RETHINK' <<<"$OUT" \
   || ! grep -q 'ABSTAIN' <<<"$OUT"; then
    echo "check-sigma-reinforce: action labels missing" >&2
    exit 6
fi

echo "check-sigma-reinforce: PASS (binary self-test rc=0)"
