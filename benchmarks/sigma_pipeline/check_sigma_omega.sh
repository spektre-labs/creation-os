#!/usr/bin/env bash
# σ-Omega smoke test (S6): Ω loop + ∫σ tracking + K_eff rollback.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_omega
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Deterministic demo: 8 iterations, σ falls 0.80 → 0.55, K_eff drops at
# iteration 5 → 1 rollback → recovers to best σ=0.47 on iteration 7.
grep -q '"iterations":8'          <<<"$OUT" || { echo "iter"      >&2; exit 5; }
grep -q '"best_iter":7'           <<<"$OUT" || { echo "best_iter" >&2; exit 6; }
grep -q '"best_sigma":0.4700'     <<<"$OUT" || { echo "best_sigma" >&2; exit 7; }
grep -q '"last_sigma":0.4700'     <<<"$OUT" || { echo "last"      >&2; exit 8; }
grep -q '"mean_sigma":0.6325'     <<<"$OUT" || { echo "mean"      >&2; exit 9; }
grep -q '"k_eff_min":0.0100'      <<<"$OUT" || { echo "k_min"     >&2; exit 10; }
grep -q '"n_improvements":8'      <<<"$OUT" || { echo "improv"    >&2; exit 11; }
grep -q '"n_rollbacks":1'         <<<"$OUT" || { echo "rollbacks" >&2; exit 12; }
grep -q '"n_save":8'              <<<"$OUT" || { echo "saves"     >&2; exit 13; }
grep -q '"n_restore":1'           <<<"$OUT" || { echo "restores"  >&2; exit 14; }
grep -q '"focus":"math"'          <<<"$OUT" || { echo "focus"     >&2; exit 15; }
grep -q '"trace_rollback_iter":5' <<<"$OUT" || { echo "rb iter"   >&2; exit 16; }

echo "check-sigma-omega: PASS (Ω loop + ∫σ tracking + best snapshot + K_eff rollback)"
