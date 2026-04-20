#!/usr/bin/env bash
#
# Smoke test for σ-Formal (H4): T3/T4/T5/T6 evidence ledger.
#
# This closes the ledger that v259 left at 2/6.  σ-Formal is a
# runtime harness, not Lean; the paper (H5) cites the witness
# counts below as the C-level evidence.

set -euo pipefail

BIN=${1:-./creation_os_sigma_formal}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-formal: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                     <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                          <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }
grep -q '"discharged":"4/4"'                   <<<"$OUT" || { echo "FAIL 4/4 discharged"; exit 1; }

# T3 monotonicity: 16384 witnesses, zero violations.
grep -q '"id":"T3","desc":"gate monotone in τ_accept","witnesses":16384,"violations":0,"discharged":true' \
  <<<"$OUT" || { echo "FAIL T3"; exit 1; }

# T4 commutativity.
grep -q '"id":"T4","desc":"gate(σ₁,τ) ∧ gate(σ₂,τ) commutes","witnesses":16384,"violations":0,"discharged":true' \
  <<<"$OUT" || { echo "FAIL T4"; exit 1; }

# T5 encode/decode idempotence: 128 iterations × 7 message types
# = 896 witnesses.
grep -q '"id":"T5","desc":"decode ∘ encode is idempotent","witnesses":896,"violations":0,"discharged":true' \
  <<<"$OUT" || { echo "FAIL T5"; exit 1; }

# T6 latency bound: 6.4M calls, observed p99 must be < bound.
grep -q '"id":"T6"'                            <<<"$OUT" || { echo "FAIL T6 id"; exit 1; }
grep -q '"witnesses":6400000'                  <<<"$OUT" || { echo "FAIL T6 witnesses"; exit 1; }
grep -q '"bound_ns":250'                       <<<"$OUT" || { echo "FAIL T6 bound"; exit 1; }

# Extract T6 p99 and check <= bound.  Portable awk.
P99=$(echo "$OUT" | awk -F'"p99_ns":' 'NF>1{ v=$2; sub(/[^0-9].*/,"",v); print v; exit }')
if [ -z "$P99" ]; then
  echo "FAIL could not parse T6 p99_ns"; exit 1
fi
if [ "$P99" -ge 250 ]; then
  echo "FAIL T6 p99 ${P99}ns ≥ 250ns bound"; exit 1
fi

echo "check-sigma-formal: T3/T4/T5/T6 all discharged (p99 gate latency ${P99}ns)"
