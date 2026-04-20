#!/usr/bin/env bash
#
# Smoke test for σ-Split (D2): distributed layer inference with
# σ-gated early exit.  Pipeline: A(0..10) → B(11..20) → C(21..32),
# τ_exit = 0.20, min_stages = 0.
#
# Scenarios:
#   easy   — σ already below threshold at A  → exit after A (B+C skipped)
#   medium — σ drops below threshold at B    → exit after B (C skipped)
#   hard   — σ stays high                    → full path

set -euo pipefail

BIN=${1:-./creation_os_sigma_split}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-split: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                                     <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                          <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }
grep -q '"stages":3'                                           <<<"$OUT" || { echo "FAIL stage count"; exit 1; }
grep -q '"tau_exit":0.20'                                      <<<"$OUT" || { echo "FAIL tau_exit"; exit 1; }
grep -q '"full_latency_ms":55.00'                              <<<"$OUT" || { echo "FAIL full latency"; exit 1; }

# Easy: only A runs, 40ms saved.
grep -q '"easy":{"stages_run":1,"early_exit":true,"exit_stage":0,"sigma":0.10,"latency_ms":15.00,"saved_ms":40.00}' \
  <<<"$OUT" || { echo "FAIL easy early exit"; exit 1; }

# Medium: A + B run, C skipped, 22ms saved.
grep -q '"medium":{"stages_run":2,"early_exit":true,"exit_stage":1,"sigma":0.15,"latency_ms":33.00,"saved_ms":22.00}' \
  <<<"$OUT" || { echo "FAIL medium early exit"; exit 1; }

# Hard: full path, no early exit.
grep -q '"hard":{"stages_run":3,"early_exit":false,"exit_stage":2,"sigma":0.25,"latency_ms":55.00}' \
  <<<"$OUT" || { echo "FAIL hard full path"; exit 1; }

echo "check-sigma-split: early-exit matrix (easy/medium/hard) green"
