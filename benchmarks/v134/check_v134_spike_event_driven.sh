#!/usr/bin/env bash
# check-v134: σ-spike event-driven gate
#
# Verifies:
#   1. self-test passes (δ-spike + burst detection + ring accounting)
#   2. demo shows stable_ratio ≥ 0.60 (the "70% savings" claim)
#   3. constructed stream: 3 spikes in 5 tokens → BURST verdict
set -euo pipefail
BIN="./creation_os_v134_spike"
[ -x "$BIN" ] || { echo "check-v134: $BIN not built"; exit 1; }

echo "check-v134: self-test"
"$BIN" --self-test

echo "check-v134: demo stable ratio"
out=$("$BIN" --demo)
ratio=$(printf '%s\n' "$out" | awk -F'[= ]' '/stable_ratio/{print $2; exit}')
awk -v r="$ratio" 'BEGIN{ if (r+0 < 0.60) { print "stable_ratio too low: "r; exit 1 } }'
echo "  stable_ratio=$ratio OK"

echo "check-v134: burst after 3 spikes in 5 tokens"
# 0.3,0.3 (bootstrap + stable), then three big jumps within 5 tokens.
out=$("$BIN" --stream 0.30,0.30,0.70,0.30,0.70)
echo "$out" | grep -q "verdict=BURST" || { echo "no BURST in stream"; exit 1; }
echo "  BURST detected in 5-token window OK"

echo "check-v134: all OK"
