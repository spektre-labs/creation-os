#!/usr/bin/env bash
#
# Smoke test for σ-Marketplace (D3): σ-filtered provider selection,
# SCSL enforcement, EWMA reputation, auto-ban.

set -euo pipefail

BIN=${1:-./creation_os_sigma_marketplace}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-marketplace: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                                    <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                         <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }
grep -q '"providers":4'                                       <<<"$OUT" || { echo "FAIL provider count"; exit 1; }
grep -q '"ban_floor":0.80'                                    <<<"$OUT" || { echo "FAIL ban floor"; exit 1; }

# Loose budget: cheapest SCSL provider wins.
grep -q '"pick_loose":"cheap"'                                <<<"$OUT" || { echo "FAIL loose select"; exit 1; }
# Tight σ (0.10): only "pro" clears it.
grep -q '"pick_tight_sigma":"pro"'                            <<<"$OUT" || { echo "FAIL tight-σ select"; exit 1; }
# Tight σ + price ≤ 0.01: nobody qualifies.
grep -q '"pick_tight_both":"null"'                            <<<"$OUT" || { echo "FAIL tight-both NULL"; exit 1; }
# Rogue (cheaper + lower σ, but SCSL=0) must be filtered out.
grep -q '"pick_rogue_filtered":true'                          <<<"$OUT" || { echo "FAIL rogue filter"; exit 1; }

# After 5× σ_result=0.95 on "mid": auto-banned, σ → 1.0 forced.
grep -q '"mid_after_badrun":{"sigma":0.9800,"banned":true,"failures":5,"successes":0}' \
  <<<"$OUT" || { echo "FAIL mid auto-ban"; exit 1; }
# Post-ban, the σ≤0.10 query routes to pro (mid no longer available).
grep -q '"pick_after_ban":"pro"'                              <<<"$OUT" || { echo "FAIL post-ban select"; exit 1; }

grep -q '"total_queries":5'                                   <<<"$OUT" || { echo "FAIL total_queries"; exit 1; }
grep -q '"total_spend_eur":0.0400'                            <<<"$OUT" || { echo "FAIL total_spend"; exit 1; }

echo "check-sigma-marketplace: selection + SCSL + auto-ban matrix green"
