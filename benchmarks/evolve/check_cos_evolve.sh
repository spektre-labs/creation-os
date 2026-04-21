#!/usr/bin/env bash
# check-cos-evolve: end-to-end harness for the OMEGA stack.
#
# Exercises every subcommand of cos-evolve against a small fixture:
#   self-test · evolve (×2 seeds) · memory-top · memory-stats
#   calibrate-auto · discover · omega (recursive) · daemon
#
# Uses an isolated COS_EVOLVE_HOME per run so repeated invocations
# of the harness stay hermetic.
set -euo pipefail
set -o errtrace

bin=./cos-evolve
[[ -x "$bin" ]] || { echo "FAIL: $bin not built"; exit 2; }

fixture=benchmarks/evolve/fixture_tiny.jsonl
hypos=benchmarks/evolve/hypotheses_smoke.jsonl
[[ -f "$fixture" ]] || { echo "FAIL: $fixture missing"; exit 2; }
[[ -f "$hypos"   ]] || { echo "FAIL: $hypos missing";   exit 2; }

home=$(mktemp -d -t cos_evolve_check_XXXXXX)
export COS_EVOLVE_HOME="$home"
trap 'rm -rf "$home"' EXIT

echo "  · cos-evolve self-test"
$bin self-test >/dev/null

echo "  · cos-evolve evolve (seed 0xA)"
out=$($bin evolve --fixture "$fixture" --generations 200 --seed 0xA --kernel sigma_router)
echo "$out" | grep -q '^  best   : brier='      || { echo "FAIL: no best-brier line"; exit 1; }
echo "$out" | grep -q '^  weights: lp='          || { echo "FAIL: no weights line";    exit 1; }
best_brier=$(echo "$out" | awk '/best.*brier/ { split($0,a,"brier="); split(a[2],b," "); print b[1] }')
start_brier=$(echo "$out" | awk '/start.*brier/ { split($0,a,"brier="); split(a[2],b," "); print b[1] }')
awk -v s="$start_brier" -v b="$best_brier" \
    'BEGIN{ if (b+1e-9 >= s) { print "FAIL: brier did not drop: " s " -> " b; exit 1 } }'

echo "  · cos-evolve evolve (seed 0xB — second run appends to memory)"
$bin evolve --fixture "$fixture" --generations 100 --seed 0xB --kernel sigma_router >/dev/null

echo "  · cos-evolve memory-top"
mt=$($bin memory-top --kernel sigma_router --limit 5)
echo "$mt" | grep -q 'accepted mutations'      || { echo "FAIL: memory-top header"; exit 1; }

echo "  · cos-evolve memory-stats"
ms=$($bin memory-stats --kernel sigma_router)
total=$(echo "$ms" | awk '/total/ { print $3 }')
[[ "$total" -ge 2 ]] || { echo "FAIL: expected >=2 rows, got $total"; exit 1; }

echo "  · cos-evolve calibrate-auto"
ca=$($bin calibrate-auto --fixture "$fixture" --mode balanced --alpha 0.5)
echo "$ca" | grep -q '^  τ:'                   || { echo "FAIL: no tau line"; exit 1; }
echo "$ca" | grep -q 'false_accept='           || { echo "FAIL: no false_accept"; exit 1; }

echo "  · cos-evolve discover (three smoke hypotheses)"
dv=$($bin discover --experiments "$hypos")
conf=$(echo "$dv" | awk '/^discover:/ { for (i=1;i<=NF;i++) if ($i ~ /^confirmed=/) { split($i,a,"="); print a[2] } }')
[[ "$conf" -ge 3 ]] || { echo "FAIL: discover confirmed=$conf, expected >=3"; echo "$dv"; exit 1; }

echo "  · cos-evolve omega (2 rounds)"
om=$($bin omega --fixture "$fixture" --rounds 2 --gens 80)
echo "$om" | grep -q 'cos-omega:'              || { echo "FAIL: no omega summary"; exit 1; }

echo "  · cos-evolve daemon (3 iters, no sleep)"
dm=$($bin daemon --fixture "$fixture" --max-iters 3 --interval 0 --gens 50)
echo "$dm" | grep -q 'daemon halt: reason='    || { echo "FAIL: no daemon halt line"; exit 1; }
[[ -f "$home/omega_daemon.log" ]]              || { echo "FAIL: daemon log missing"; exit 1; }

echo "check-cos-evolve: PASS (self-test + evolve + memory + calibrate + discover + omega + daemon)"
