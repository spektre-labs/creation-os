#!/usr/bin/env bash
# Smoke test for σ-TTT primitive.
set -euo pipefail

cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_ttt
[[ -x "$BIN" ]] || { echo "missing binary: $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0'   <<<"$OUT" || { echo "self_test_rc != 0" >&2; exit 3; }
grep -q '"pass":true'        <<<"$OUT" || { echo "pass != true" >&2; exit 4; }
# Demo: 4 steps submitted, 3 updated (1 skipped at σ=0.10).
grep -q '"n_steps_total":4'      <<<"$OUT" || { echo "n_steps_total != 4" >&2; exit 5; }
grep -q '"n_steps_updated":3'    <<<"$OUT" || { echo "n_steps_updated != 3" >&2; exit 6; }
# Drift must be > tau_drift=0.25 AND reset must have fired exactly once.
grep -q '"did_reset":true'   <<<"$OUT" || { echo "did_reset != true" >&2; exit 7; }
grep -q '"n_resets":1'       <<<"$OUT" || { echo "n_resets != 1"     >&2; exit 8; }

echo "check-sigma-ttt: PASS (binary self-test rc=0; 3/4 steps updated; drift-reset fired once)"
