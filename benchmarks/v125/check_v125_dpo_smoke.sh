#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v125 σ-DPO merge-gate smoke.

set -euo pipefail
BIN=./creation_os_v125_dpo
test -x "$BIN" || { echo "missing $BIN — run 'make creation_os_v125_dpo' first"; exit 1; }

echo "check-v125: self-test"
"$BIN" --self-test

echo "check-v125: DPO loss — preferred vs anti-preferred"
# δ = 1.0 · (7.9 − 0) = 7.9 → L ≈ 3.7e-4
PREF=$("$BIN" --loss 1.0 -0.1 -8.0 -4.0 -4.0)
echo "  $PREF"
echo "$PREF" | grep -q '"pref_prob":0.999'
# δ = 1.0 · (−7.9 − 0) = −7.9 → L ≈ 7.9
ANTI=$("$BIN" --loss 1.0 -8.0 -0.1 -4.0 -4.0)
echo "  $ANTI"
echo "$ANTI" | grep -q '"pref_prob":0.000'

# δ = 0 → L = log 2 ≈ 0.6931
ZERO=$("$BIN" --loss 0.1 -3.0 -3.0 -3.0 -3.0)
echo "  $ZERO"
echo "$ZERO" | grep -q '"loss":0.693147'
echo "$ZERO" | grep -q '"pref_prob":0.500000'

echo "check-v125: σ-dataset labeling"
LBL=$("$BIN" --label-synthetic)
echo "  $LBL"
echo "$LBL" | grep -q '"pairs_emitted":2'
echo "$LBL" | grep -q '"pairs_from_correction":1'
echo "$LBL" | grep -q '"pairs_from_sigma":1'
echo "$LBL" | grep -q '"rows_skipped_mid_sigma":1'
echo "$LBL" | grep -q '"rows_skipped_unpaired":1'
# The correction chose text is "six" (student said "four")
echo "$LBL" | grep -q '"chosen":"six"'
echo "$LBL" | grep -q '"rejected":"four"'
# σ-pair chose the low-σ answer (tallinn) over stockholm
echo "$LBL" | grep -q '"chosen":"tallinn"'
echo "$LBL" | grep -q '"rejected":"stockholm"'

echo "check-v125: mode-collapse demo"
COL=$("$BIN" --collapse-demo)
echo "  $COL"
echo "$COL" | grep -q '"verdict_ok":"ok"'
echo "$COL" | grep -q '"verdict_collapse":"collapse"'

echo "check-v125: OK (DPO loss analytic limits + σ-labeling + collapse rollback signal)"
