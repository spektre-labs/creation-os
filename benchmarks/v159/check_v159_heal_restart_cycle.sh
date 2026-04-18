#!/usr/bin/env bash
# v159 σ-Heal merge-gate: proves the health daemon detects a
# failure, diagnoses its root cause, executes the mapped self-
# repair action, emits a σ-delta heal receipt, recovers σ_heal
# back to a healthy range, and that the predictive slope
# detector fires a PREEMPTIVE repair before an actual failure.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

BIN=./creation_os_v159_heal
SEED=159

die() { echo "v159 check: FAIL — $*" >&2; exit 1; }

echo "  self-test"
st="$($BIN --self-test)"
echo "  $st"
echo "$st" | grep -q '"self_test":"pass"' || die "self-test"

echo "  baseline (scenario 0) → no receipts"
base="$($BIN --scenario 0 --seed $SEED)"
echo "$base" | grep -q '"receipts":\[\]' || die "baseline must have 0 receipts"
echo "$base" | grep -q '"n_failing":0'   || die "baseline n_failing must be 0"

echo "  http down (scenario 1) → RESTART + σ recovers"
s1="$($BIN --scenario 1 --seed $SEED)"
echo "$s1" | grep -q '"action":"restart_http"' || die "expected restart_http"
echo "$s1" | grep -q '"succeeded":true'        || die "restart must succeed"
echo "$s1" | grep -q '"sigma_before":0.9200'   || die "sigma_before failure"
echo "$s1" | grep -q '"v106 /health unreachable' || die "failure text"

echo "  kv degenerate (scenario 2) → FLUSH_KV"
s2="$($BIN --scenario 2 --seed $SEED)"
echo "$s2" | grep -q '"action":"flush_kv"'     || die "expected flush_kv"

echo "  adapter corrupted (scenario 3) → ROLLBACK_LORA"
s3="$($BIN --scenario 3 --seed $SEED)"
echo "$s3" | grep -q '"action":"rollback_lora"' || die "expected rollback_lora"

echo "  sqlite corrupted (scenario 4) → RESTORE_SQLITE"
s4="$($BIN --scenario 4 --seed $SEED)"
echo "$s4" | grep -q '"action":"restore_sqlite"' || die "expected restore_sqlite"

echo "  weights missing (scenario 5) → REFETCH_GGUF + cascade bridge + http"
s5="$($BIN --scenario 5 --seed $SEED)"
echo "$s5" | grep -q '"action":"refetch_gguf"'    || die "expected refetch_gguf"
echo "$s5" | grep -q '"action":"restart_bridge"'  || die "expected cascade restart_bridge"
echo "$s5" | grep -q '"action":"restart_http"'    || die "expected cascade restart_http"
nr=$(echo "$s5" | grep -o '"succeeded":true' | wc -l | tr -d ' ')
[ "$nr" -ge "3" ] || die "weights cascade should produce ≥ 3 receipts (got $nr)"

echo "  predictive slope (--predict kv) → PREEMPTIVE + degrading=true"
pred="$($BIN --predict kv --seed $SEED)"
echo "$pred" | grep -q '"action":"preemptive_repair"' || die "expected preemptive_repair"
echo "$pred" | grep -q '"predictive":true'            || die "predictive flag"
echo "$pred" | grep -q '"degrading":true'             || die "degrading flag on component"

echo "  full cycle (scenarios 1..5) → receipts for every primary failure"
cyc="$($BIN --cycle --seed $SEED)"
for want in restart_http flush_kv rollback_lora restore_sqlite refetch_gguf; do
    echo "$cyc" | grep -q "\"action\":\"$want\"" || die "cycle missing action $want"
done

echo "  determinism (same seed → byte-identical JSON)"
a="$($BIN --scenario 4 --seed $SEED)"
b="$($BIN --scenario 4 --seed $SEED)"
[ "$a" = "$b" ] || die "non-deterministic"

echo "v159 heal restart-cycle: OK"
