#!/usr/bin/env bash
#
# Smoke test for σ-Substrate (H3): vtable + cross-substrate
# equivalence across digital / bitnet / spike / photonic.

set -euo pipefail

BIN=${1:-./creation_os_sigma_substrate}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-substrate: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                                    <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                         <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }

# --- Dominant activations [.02 .03 .90 .05], τ=0.30 ---
# Every backend must land on ACCEPT and the equivalence bit is set.
grep -q '"case":"dominant","tau_accept":0.30,"equivalent":true' <<<"$OUT" \
  || { echo "FAIL dominant equivalence"; exit 1; }
grep -q '"digital":{"sigma":0.1000,"accept":1}'                 <<<"$OUT" \
  || { echo "FAIL dominant digital σ"; exit 1; }
grep -q '"bitnet":{"sigma":0.0000,"accept":1}'                  <<<"$OUT" \
  || { echo "FAIL dominant bitnet σ"; exit 1; }
grep -q '"spike":{"sigma":0.0000,"accept":1}'                   <<<"$OUT" \
  || { echo "FAIL dominant spike σ"; exit 1; }
grep -q '"photonic":{"sigma":0.1000,"accept":1}'                <<<"$OUT" \
  || { echo "FAIL dominant photonic σ"; exit 1; }

# --- Middle activations [.10 .10 .70 .10], τ=0.50 ---
grep -q '"case":"middle","tau_accept":0.50,"equivalent":true'   <<<"$OUT" \
  || { echo "FAIL middle equivalence"; exit 1; }
grep -q '"digital":{"sigma":0.3000,"accept":1}'                 <<<"$OUT" \
  || { echo "FAIL middle digital σ"; exit 1; }
grep -q '"photonic":{"sigma":0.3000,"accept":1}'                <<<"$OUT" \
  || { echo "FAIL middle photonic σ"; exit 1; }

# --- Uniform activations [.25 .25 .25 .25], τ=0.30 ---
grep -q '"case":"uniform","tau_accept":0.30,"equivalent":true'  <<<"$OUT" \
  || { echo "FAIL uniform equivalence"; exit 1; }
# All four backends must agree at σ = 0.75 (= 1 − 1/4).
grep -Eo '"uniform"[^}]*\}[^}]*\}[^}]*\}[^}]*\}[^}]*\}' <<<"$OUT" \
  | grep -q '"digital":{"sigma":0.7500,"accept":0}'             \
  || { echo "FAIL uniform digital σ"; exit 1; }
grep -Eo '"uniform"[^}]*\}[^}]*\}[^}]*\}[^}]*\}[^}]*\}' <<<"$OUT" \
  | grep -q '"bitnet":{"sigma":0.7500,"accept":0}'              \
  || { echo "FAIL uniform bitnet σ"; exit 1; }
grep -Eo '"uniform"[^}]*\}[^}]*\}[^}]*\}[^}]*\}[^}]*\}' <<<"$OUT" \
  | grep -q '"spike":{"sigma":0.7500,"accept":0}'               \
  || { echo "FAIL uniform spike σ"; exit 1; }
grep -Eo '"uniform"[^}]*\}[^}]*\}[^}]*\}[^}]*\}[^}]*\}' <<<"$OUT" \
  | grep -q '"photonic":{"sigma":0.7500,"accept":0}'            \
  || { echo "FAIL uniform photonic σ"; exit 1; }

echo "check-sigma-substrate: digital/bitnet/spike/photonic agree on dominant + middle + uniform"
