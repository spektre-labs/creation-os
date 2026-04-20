#!/usr/bin/env bash
# σ-Synthetic smoke test (S3): quality gate + anchor mix + collapse guard.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_synthetic
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Demo A — quality gate: target 5, τ=0.30, anchor 0.20.
# Script of 10 candidates ends with 1 real pair.
# Accepts: alpha, beta, delta, zeta (4 synth), REAL_anchor_1 (1 real).
# Rejects: gamma/epsilon/iota on σ; eta/theta on anchor cap.
grep -q '"n_target":5'          <<<"$OUT" || { echo "n_target" >&2; exit 5; }
grep -q '"attempts":10'         <<<"$OUT" || { echo "attempts" >&2; exit 6; }
grep -q '"accepted":5'          <<<"$OUT" || { echo "accepted" >&2; exit 7; }
grep -q '"n_real":1'            <<<"$OUT" || { echo "n_real"   >&2; exit 8; }
grep -q '"n_synthetic":4'       <<<"$OUT" || { echo "n_synth"  >&2; exit 9; }
grep -q '"rejected_quality":3'  <<<"$OUT" || { echo "rej_qual" >&2; exit 10; }
grep -q '"rejected_anchor":2'   <<<"$OUT" || { echo "rej_anch" >&2; exit 11; }
grep -q '"acceptance_rate":0.5000' <<<"$OUT" || { echo "rate"  >&2; exit 12; }
grep -q '"quality_gate":{[^}]*"collapsed":false' <<<"$OUT" \
    || { echo "not-collapsed" >&2; exit 13; }

# Demo B — collapse detector: all outputs share 8-byte prefix
# "collapse" → σ_diversity = 1/N collapses below τ=0.50 at the
# third accept, loop stops.
grep -q '"collapse_guard":{"accepted":3' <<<"$OUT" \
    || { echo "collapse accepted" >&2; exit 14; }
grep -q '"collapse_guard":{[^}]*"diversity":0.3333' <<<"$OUT" \
    || { echo "collapse diversity" >&2; exit 15; }
grep -q '"collapse_guard":{[^}]*"collapsed":true'   <<<"$OUT" \
    || { echo "collapsed flag" >&2; exit 16; }

echo "check-sigma-synthetic: PASS (quality gate + anchor mix + collapse guard)"
