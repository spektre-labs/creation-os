#!/usr/bin/env bash
#
# Smoke test for σ-Photonic (H2): intensity → σ on an optical
# mesh (dominant / uniform / Mach-Zehnder).

set -euo pipefail

BIN=${1:-./creation_os_sigma_photonic}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-photonic: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                                  <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                       <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }

# Dominant: channel 2 holds 0.90/1.00 → σ = 1 − 0.9 = 0.10.
grep -q '"dominant":{"channels":4,"total":1.0000,"max":0.9000,"argmax":2,"sigma":0.1000}' \
  <<<"$OUT" || { echo "FAIL dominant σ"; exit 1; }

# Uniform: 4 equal channels → σ = 1 − 1/4 = 0.75.
grep -q '"uniform":{"channels":4,"sigma":0.7500}'           <<<"$OUT" || { echo "FAIL uniform σ"; exit 1; }

# Mach-Zehnder with one constructive arm (Δφ=0, V=1) collapses
# all intensity onto that arm → σ = 0.
grep -q '"mach_zehnder":{"channels":4,"visibility":1.00,"arm0_intensity":1.0000,"arm1_intensity":0.0000,"sigma":0.0000}' \
  <<<"$OUT" || { echo "FAIL MZI σ"; exit 1; }

echo "check-sigma-photonic: intensity ratio σ across dominant / uniform / MZI green"
