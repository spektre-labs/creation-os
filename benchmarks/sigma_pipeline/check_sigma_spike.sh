#!/usr/bin/env bash
#
# Smoke test for σ-Spike (H1): LIF neuron + σ mapping + digital
# equivalence + population σ.

set -euo pipefail

BIN=${1:-./creation_os_sigma_spike}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-spike: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                                    <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                         <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }

# Single neuron: threshold 1.0, decay 0.9, 10 inputs, first spike
# lands at index 4 (cumulative leaky integrator crosses threshold).
grep -q '"threshold":1.00,"decay":0.90'                       <<<"$OUT" || { echo "FAIL neuron cfg"; exit 1; }
grep -q '"inputs":10,"total_spikes":2,"total_steps":10'       <<<"$OUT" || { echo "FAIL spike count"; exit 1; }
grep -q '"first_spike_step":4'                                <<<"$OUT" || { echo "FAIL first_spike"; exit 1; }
grep -q '"final_sigma":0.2200'                                <<<"$OUT" || { echo "FAIL final sigma"; exit 1; }

# Population: silent inputs (0.05) never cross threshold → σ ≈ 1.
grep -q '"sigma_silent":1.0000'                               <<<"$OUT" || { echo "FAIL silent sigma"; exit 1; }
# Burst inputs (1.5) fire every step → σ = 0.
grep -q '"sigma_burst":0.0000'                                <<<"$OUT" || { echo "FAIL burst sigma"; exit 1; }

echo "check-sigma-spike: LIF + σ-mapping + digital equivalence + population σ all green"
