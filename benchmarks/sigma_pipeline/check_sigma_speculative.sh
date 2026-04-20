#!/usr/bin/env bash
# Smoke test for the σ-speculative primitive binary.
set -euo pipefail

cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_speculative
if [[ ! -x "$BIN" ]]; then
    echo "check-sigma-speculative: binary missing: $BIN" >&2
    exit 2
fi

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0'    <<<"$OUT" \
    || { echo "check-sigma-speculative: self_test_rc != 0" >&2; exit 3; }
grep -q '"pass":true'         <<<"$OUT" \
    || { echo "check-sigma-speculative: pass != true"       >&2; exit 4; }
grep -q '"LOCAL"'             <<<"$OUT" \
    || { echo "check-sigma-speculative: LOCAL label missing"    >&2; exit 5; }
grep -q '"ESCALATE"'          <<<"$OUT" \
    || { echo "check-sigma-speculative: ESCALATE label missing" >&2; exit 6; }
# Savings for 10% escalation at 0.0005€ local / 0.02€ API must be in
# the (0.87, 0.88) band; this is the one deterministic number that
# would move if the cost formula regressed.
if ! grep -qE '"savings":0\.87[0-9]+' <<<"$OUT"; then
    echo "check-sigma-speculative: cost_savings out of expected band" >&2
    echo "  expected \"savings\":0.87… got: $OUT" >&2
    exit 7
fi

echo "check-sigma-speculative: PASS (binary self-test rc=0, savings in band)"
