#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v152 merge-gate: corpus → 200 QA → simulated SFT → ≥15 % σ drop.
set -euo pipefail

BIN=./creation_os_v152_distill
[[ -x "$BIN" ]] || { echo "missing binary $BIN"; exit 1; }

# -----------------------------------------------------------------
# T1: self-test passes (K1..K4).
# -----------------------------------------------------------------
out=$("$BIN" --self-test)
echo "  self-test: $out"
echo "$out" | grep -q '"self_test":"pass"' || { echo "FAIL: self-test"; exit 1; }

# -----------------------------------------------------------------
# T2: corpus-info reports 16 papers and all 8 topics covered.
# -----------------------------------------------------------------
info=$("$BIN" --corpus-info)
echo "  corpus: $info"
echo "$info" | grep -q '"n_papers":16' || { echo "FAIL: expected 16 papers"; exit 1; }
uncovered=$( (echo "$info" | grep -o '"covered":false' || true) | wc -l | tr -d ' ')
[[ "$uncovered" == "0" ]] || { echo "FAIL: uncovered topic count=$uncovered"; exit 1; }

# -----------------------------------------------------------------
# T3: distillation run — n=200, coverage=0.50 → ≥15 % drop on
#     covered subset, ≤1 % delta on non-covered subset.
# -----------------------------------------------------------------
rep=$("$BIN" --distill --n 200 --coverage 0.50 --seed 152)
echo "  report: $rep"
echo "$rep" | grep -q '"passed":true' || { echo "FAIL: passed=false: $rep"; exit 1; }

drop=$(echo "$rep" | sed -E 's/.*"sigma_drop_pct_covered":([0-9.\-]+).*/\1/')
awk -v d="$drop" 'BEGIN{exit !(d+0 >= 15.0)}' \
    || { echo "FAIL: drop $drop < 15 %"; exit 1; }

delta=$(echo "$rep" | sed -E 's/.*"sigma_delta_non_covered":([0-9.\-]+).*/\1/')
awk -v x="$delta" 'BEGIN{exit !(x+0 <= 0.01)}' \
    || { echo "FAIL: non-covered σ rose by $delta > 0.01"; exit 1; }

n_covered=$(echo "$rep" | sed -E 's/.*"n_covered":([0-9]+).*/\1/')
[[ "$n_covered" -ge 100 ]] || { echo "FAIL: n_covered=$n_covered < 100"; exit 1; }

# -----------------------------------------------------------------
# T4: higher coverage → larger σ drop (monotonicity).
# -----------------------------------------------------------------
low=$( "$BIN" --distill --n 200 --coverage 0.30 --seed 152 \
        | sed -E 's/.*"sigma_drop_pct_covered":([0-9.\-]+).*/\1/')
high=$("$BIN" --distill --n 200 --coverage 0.80 --seed 152 \
        | sed -E 's/.*"sigma_drop_pct_covered":([0-9.\-]+).*/\1/')
echo "  drop@0.30=$low  drop@0.80=$high"
awk -v a="$low" -v b="$high" \
    'BEGIN{exit !(b+0 > a+0 + 5.0)}' \
    || { echo "FAIL: drop not monotone in coverage"; exit 1; }

# -----------------------------------------------------------------
# T5: determinism — same seed + knobs → byte-identical JSON.
# -----------------------------------------------------------------
a=$("$BIN" --distill --n 200 --coverage 0.50 --seed 42)
b=$("$BIN" --distill --n 200 --coverage 0.50 --seed 42)
[[ "$a" == "$b" ]] || { echo "FAIL: v152 not deterministic"; exit 1; }

echo "v152 corpus→QA→SFT smoke: OK"
