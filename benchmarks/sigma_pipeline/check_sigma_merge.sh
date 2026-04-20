#!/usr/bin/env bash
# σ-Merge smoke test (A3): 4 methods + α-sweep + CONSTRUCTIVE gate.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_merge
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Method list pins.
grep -q '"methods":\["LINEAR","SLERP","TIES","TASK_ARITH"\]' <<<"$OUT" \
    || { echo "methods" >&2; exit 5; }

# Constructive scenario: a=+1s, b=-1s, target=0; α=0.5 → σ_after=0.
grep -q '"constructive_single":{"method":"LINEAR","alpha":0.5000,"sigma_a":0.5000,"sigma_b":0.5000,"sigma_after":0.0000,"constructive":true}' <<<"$OUT" \
    || { echo "constructive single" >&2; exit 6; }
grep -q '"constructive_sweep":{"best_alpha":0.5000,"sigma_after":0.0000,"constructive":true}' <<<"$OUT" \
    || { echo "constructive sweep" >&2; exit 7; }

# Destructive single: α=0.5 midpoint of (0,0) and (2,2) at scale 4
# → distance 2 → σ = 0.5 which is NOT < min(σ_a=0, σ_b=1). constructive=false.
grep -q '"destructive_single":{"alpha":0.5000,"sigma_a":0.0000,"sigma_b":1.0000,"sigma_after":0.5000,"constructive":false}' <<<"$OUT" \
    || { echo "destructive single" >&2; exit 8; }

# Destructive sweep picks α=1.0 (pure-a = target), σ_after=0 → constructive false
# (σ_after < min(σ_a, σ_b)? 0 < min(0, 1) = 0 → strict less than: false).
grep -q '"destructive_sweep":{"best_alpha":1.0000,"sigma_after":0.0000,"constructive":false}' <<<"$OUT" \
    || { echo "destructive sweep" >&2; exit 9; }

echo "check-sigma-merge: PASS (4 methods, α-sweep picks min, CONSTRUCTIVE strictly <)"
