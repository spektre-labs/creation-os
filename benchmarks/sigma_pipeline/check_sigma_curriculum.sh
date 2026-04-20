#!/usr/bin/env bash
# σ-Curriculum smoke test (S2): promote / reset / max-cap state machine.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_curriculum
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Demo ladder config: 5 levels, min_rounds=2, τ=0.30.
grep -q '"max_level":5'        <<<"$OUT" || { echo "max_level" >&2; exit 5; }
grep -q '"min_rounds":2'       <<<"$OUT" || { echo "min_rounds" >&2; exit 6; }
grep -q '"sigma_threshold":0.30' <<<"$OUT" || { echo "τ"       >&2; exit 7; }

# 14-step stream: 4 promotions, 2 resets, 1 maxed, 12 successes.
grep -q '"rounds":14'          <<<"$OUT" || { echo "rounds"    >&2; exit 8; }
grep -q '"promotions":4'       <<<"$OUT" || { echo "promos"    >&2; exit 9; }
grep -q '"resets":2'           <<<"$OUT" || { echo "resets"    >&2; exit 10; }
grep -q '"maxed":1'            <<<"$OUT" || { echo "maxed"     >&2; exit 11; }
grep -q '"total_success":12'   <<<"$OUT" || { echo "success"   >&2; exit 12; }

# Final level pinned at META cap.
grep -q '"final_level":5'             <<<"$OUT" || { echo "final level" >&2; exit 13; }
grep -q '"final_level_name":"META"'   <<<"$OUT" || { echo "level name"  >&2; exit 14; }
grep -q '"last_promotion_at":12'      <<<"$OUT" || { echo "last promo"  >&2; exit 15; }

# 12 / 14 ≈ 0.8571 acceptance.
grep -q '"acceptance_rate":0.8571' <<<"$OUT" || { echo "rate"  >&2; exit 16; }

echo "check-sigma-curriculum: PASS (5-level ladder + promote/reset/maxed)"
