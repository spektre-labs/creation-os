#!/usr/bin/env bash
# Smoke test for σ-MoE primitive.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_moe
[[ -x "$BIN" ]] || { echo "missing binary: $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0'         <<<"$OUT" || { echo "self_test_rc != 0" >&2; exit 3; }
grep -q '"pass":true'              <<<"$OUT" || { echo "pass != true"      >&2; exit 4; }

# Canonical demo: 4 experts with logits [0.10, 0.90, 0.30, 0.70].
# Argmax = expert 1 at every σ.  All 4 demo rows must name expert_id 1.
count=$(grep -o '"expert_id":1' <<<"$OUT" | wc -l | tr -d ' ')
[[ "$count" -ge 4 ]] || { echo "expected ≥4 'expert_id:1' rows, got $count" >&2; exit 5; }

# σ=0.05 → width Q (0.25).  σ=0.20 → H (0.5).  σ=0.40 → TQ (0.75).
# σ=0.80 → F (1.0).  Check each appears.
grep -q '"width":"Q"'   <<<"$OUT" || { echo "missing Q"   >&2; exit 6; }
grep -q '"width":"H"'   <<<"$OUT" || { echo "missing H"   >&2; exit 7; }
grep -q '"width":"TQ"'  <<<"$OUT" || { echo "missing TQ"  >&2; exit 8; }
grep -q '"width":"F"'   <<<"$OUT" || { echo "missing F"   >&2; exit 9; }

# Compute saved = mean(1 - 0.25, 1 - 0.5, 1 - 0.75, 1 - 1.0) = 0.375
grep -q '"compute_saved":0.3750' <<<"$OUT" || { echo "compute_saved != 0.3750" >&2; exit 10; }

# Top-K demo: K=2 → [expert 1, expert 3] (logits 0.9, 0.7).
grep -q '"experts":\[1,3\]' <<<"$OUT" || { echo "top-k experts mismatch" >&2; exit 11; }

echo "check-sigma-moe: PASS (binary self-test rc=0; width gate + top-k + 37.5% saved)"
