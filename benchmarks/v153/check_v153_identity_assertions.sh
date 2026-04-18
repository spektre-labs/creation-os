#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v153 merge-gate: σ-calibrated identity assertions + zero false positives.
set -euo pipefail

BIN=./creation_os_v153_identity
[[ -x "$BIN" ]] || { echo "missing binary $BIN"; exit 1; }

# -----------------------------------------------------------------
# T1: self-test (all five contracts).
# -----------------------------------------------------------------
out=$("$BIN" --self-test)
echo "  self-test: $out"
echo "$out" | grep -q '"self_test":"pass"' || { echo "FAIL: self-test"; exit 1; }

# -----------------------------------------------------------------
# T2: full evaluation — 10/10 correct, 0 false positives, I1..I5.
# -----------------------------------------------------------------
rep=$("$BIN" --evaluate --seed 153)
echo "  report: $rep"
echo "$rep" | grep -q '"total":10'            || { echo "FAIL: total != 10"; exit 1; }
echo "$rep" | grep -q '"correct":10'          || { echo "FAIL: correct != 10"; exit 1; }
echo "$rep" | grep -q '"false_positives":0'   || { echo "FAIL: false_positives != 0"; exit 1; }
echo "$rep" | grep -q '"I1":true'             || { echo "FAIL: I1"; exit 1; }
echo "$rep" | grep -q '"I2":true'             || { echo "FAIL: I2"; exit 1; }
echo "$rep" | grep -q '"I3":true'             || { echo "FAIL: I3"; exit 1; }
echo "$rep" | grep -q '"I4":true'             || { echo "FAIL: I4"; exit 1; }
echo "$rep" | grep -q '"I5":true'             || { echo "FAIL: I5"; exit 1; }
echo "$rep" | grep -q '"all_contracts":true'  || { echo "FAIL: all_contracts"; exit 1; }

# -----------------------------------------------------------------
# T3: dump must show TRUE facts at σ < 0.30, UNMEASURED at σ > 0.50.
# -----------------------------------------------------------------
dump=$("$BIN" --dump --seed 153)
# All 10 assertions present.
n_assert=$(echo "$dump" | grep -o '"truth":"' | wc -l | tr -d ' ')
[[ "$n_assert" == "10" ]] || { echo "FAIL: expected 10 assertions, got $n_assert"; exit 1; }
# Every "true" assertion has answer "yes".
bad_true=$( (echo "$dump" | grep -oE '"truth":"true","sigma":[^,]+,"answer":"(no|unknown)"' || true) | wc -l | tr -d ' ')
[[ "$bad_true" == "0" ]] || { echo "FAIL: some TRUE assertion not accepted"; exit 1; }
# Every "false" assertion has answer "no".
bad_false=$( (echo "$dump" | grep -oE '"truth":"false","sigma":[^,]+,"answer":"(yes|unknown)"' || true) | wc -l | tr -d ' ')
[[ "$bad_false" == "0" ]] || { echo "FAIL: some FALSE assertion not rejected"; exit 1; }
# Every "unmeasured" assertion has answer "unknown".
bad_unmeas=$( (echo "$dump" | grep -oE '"truth":"unmeasured","sigma":[^,]+,"answer":"(yes|no)"' || true) | wc -l | tr -d ' ')
[[ "$bad_unmeas" == "0" ]] || { echo "FAIL: some UNMEASURED not flagged"; exit 1; }

# -----------------------------------------------------------------
# T4: determinism.
# -----------------------------------------------------------------
a=$("$BIN" --evaluate --seed 42)
b=$("$BIN" --evaluate --seed 42)
[[ "$a" == "$b" ]] || { echo "FAIL: v153 not deterministic"; exit 1; }

# -----------------------------------------------------------------
# T5: jitter robustness — evaluate under 5 different seeds, all
#     must still pass all contracts (no seed sneaks a false pos).
# -----------------------------------------------------------------
for sd in 1 2 3 4 5; do
    r=$("$BIN" --evaluate --seed $sd)
    echo "$r" | grep -q '"all_contracts":true' \
        || { echo "FAIL: seed=$sd broke contracts: $r"; exit 1; }
done

echo "v153 identity smoke: OK"
