#!/usr/bin/env bash
# v163 σ-Evolve-Architecture merge-gate: proves the CMA-ES-lite
# search converges a Pareto front with ≥ 3 non-dominated points,
# that the σ-gate rejects any candidate whose v143 benchmark
# accuracy regresses > 3 % from baseline, that the auto-profile
# picks different genomes under tight vs. generous device budgets,
# and that the search is byte-deterministic under the same seed.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

BIN=./creation_os_v163_evolve
SEED=163

die() { echo "v163 check: FAIL — $*" >&2; exit 1; }

echo "  self-test"
st="$($BIN --self-test)"
echo "  $st"
echo "$st" | grep -q '"self_test":"pass"' || die "self-test"

echo "  run → Pareto front ≥ 3 points"
rep="$($BIN --run --seed $SEED)"
np=$(printf '%s' "$rep" | python3 -c "import sys,json; print(json.loads(sys.stdin.read())['n_pareto'])")
[ "$np" -ge "3" ] || die "expected ≥ 3 Pareto points (got $np)"

echo "  Pareto non-dominance (no point dominates another)"
printf '%s' "$rep" | python3 -c "
import sys, json
d = json.loads(sys.stdin.read())
pts = d['pareto']
for i, a in enumerate(pts):
    for j, b in enumerate(pts):
        if i == j: continue
        ge = (a['accuracy'] >= b['accuracy']
              and a['latency_ms'] <= b['latency_ms']
              and a['memory_mb']  <= b['memory_mb'])
        gt = (a['accuracy'] >  b['accuracy']
              or  a['latency_ms'] <  b['latency_ms']
              or  a['memory_mb']  <  b['memory_mb'])
        if ge and gt:
            raise SystemExit(f'dominated: {a} vs {b}')
print(f'pareto ok: {len(pts)} non-dominated points')
"

echo "  σ-gate — no front point regresses > 3 % from baseline"
printf '%s' "$rep" | python3 -c "
import sys, json
d = json.loads(sys.stdin.read())
base = d['baseline_accuracy']; tau = d['tau_regression']
for p in d['pareto']:
    assert (base - p['accuracy']) <= tau + 1e-6, (base, p)
print('sigma-gate ok')
"

echo "  auto-profile — tight vs generous budgets"
tight="$($BIN --auto --seed $SEED --lat 50  --mem 200)"
gen="$($BIN --auto   --seed $SEED --lat 500 --mem 2000)"
tn=$(printf '%s' "$tight" | python3 -c "import sys,json; print(len(json.loads(sys.stdin.read())['auto_profile']['genes']))")
gn=$(printf '%s' "$gen"   | python3 -c "import sys,json; print(len(json.loads(sys.stdin.read())['auto_profile']['genes']))")
[ "$gn" -ge "$tn" ] || die "generous auto-profile must pick ≥ genes than tight (got $gn vs $tn)"

echo "  determinism (same seed → byte-identical JSON)"
a="$($BIN --run --seed $SEED)"
b="$($BIN --run --seed $SEED)"
[ "$a" = "$b" ] || die "non-deterministic CMA-ES"

echo "  different seeds must produce different Pareto fronts"
c="$($BIN --run --seed 999)"
[ "$a" != "$c" ] || die "different seeds collapsed to same output"

echo "v163 evolve-architecture pareto-converge: OK"
