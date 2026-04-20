#!/usr/bin/env bash
#
# Smoke test for σ-Federation (D4): σ-weighted Δweight aggregation
# with σ-gate + poison guard + abstain.

set -euo pipefail

BIN=${1:-./creation_os_sigma_federation}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-federation: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                       <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                            <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }
grep -q '"updates":5'                            <<<"$OUT" || { echo "FAIL update count"; exit 1; }
grep -q '"n_params":4'                           <<<"$OUT" || { echo "FAIL n_params"; exit 1; }
grep -q '"sigma_reject":0.70'                    <<<"$OUT" || { echo "FAIL sigma_reject"; exit 1; }
grep -q '"poison_scale":3.00'                    <<<"$OUT" || { echo "FAIL poison_scale"; exit 1; }

# Two rejections: 1 σ (malicious), 1 poison (huge norm).
grep -q '"accepted":3'                           <<<"$OUT" || { echo "FAIL accepted"; exit 1; }
grep -q '"rejected_sigma":1'                     <<<"$OUT" || { echo "FAIL sigma reject"; exit 1; }
grep -q '"rejected_poison":1'                    <<<"$OUT" || { echo "FAIL poison reject"; exit 1; }
grep -q '"abstained":false'                      <<<"$OUT" || { echo "FAIL abstain flag"; exit 1; }

# Aggregated Δ is bounded near the trusted cohort (O(0.1)),
# nowhere near the malicious (5.0) or poison (50.0) magnitudes.
grep -q '"delta":\[0.1253,0.0626,0.0289,0.0508\]' <<<"$OUT" || { echo "FAIL delta values"; exit 1; }
grep -q '"delta_l2":0.1518'                       <<<"$OUT" || { echo "FAIL delta_l2"; exit 1; }

# Weight totals: (1-0.10)·200 + (1-0.15)·150 + (1-0.30)·100
#              = 180 + 127.5 + 70 = 377.5
grep -q '"total_weight":377.50'                   <<<"$OUT" || { echo "FAIL total_weight"; exit 1; }

grep -q '"accepted_ids":\["trusted-1","trusted-2","drift"\]' \
  <<<"$OUT" || { echo "FAIL accepted ids"; exit 1; }

echo "check-sigma-federation: σ-gate + poison guard + weighted aggregate green"
