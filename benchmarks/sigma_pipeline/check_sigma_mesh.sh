#!/usr/bin/env bash
#
# Smoke test for σ-Mesh (D1): peer registry, σ-weighted routing,
# EWMA reputation, abstain behaviour.
#
# Pins the canonical 4-node demo: self, strong (σ=0.10,lat=40),
# slow (σ=0.15,lat=300), bad (σ=0.90,lat=20).

set -euo pipefail

BIN=${1:-./creation_os_sigma_mesh}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-mesh: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                                    <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                         <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }
grep -q '"count":4'                                           <<<"$OUT" || { echo "FAIL node count"; exit 1; }
grep -q '"weights":{"sigma":0.70,"latency":0.30}'             <<<"$OUT" || { echo "FAIL routing weights"; exit 1; }
grep -q '"tau_abstain":0.80'                                  <<<"$OUT" || { echo "FAIL abstain floor"; exit 1; }

# High-σ local → routes to the "strong" peer (lowest σ_capability).
grep -q '"route_local_high":"strong"'                         <<<"$OUT" || { echo "FAIL route high-sigma"; exit 1; }
# Low-σ local + allow_self → self wins via preference bonus.
grep -q '"route_local_low_allow_self":"self"'                 <<<"$OUT" || { echo "FAIL route low-sigma self"; exit 1; }

# Post-success EWMA: σ drops toward σ_result (0.05), latency toward 30ms.
# α=0.80, initial σ=0.10 → new σ = 0.80·0.10 + 0.20·0.05 = 0.09.
grep -q '"strong_after_success":{"sigma":0.0900,"latency_ms":38.00,"successes":1,"failures":0}' \
  <<<"$OUT" || { echo "FAIL strong EWMA"; exit 1; }

# Three malicious rounds on "bad": 0.90 → 0.728 → 0.782 → ... ≈0.9488.
grep -q '"bad_after_failures":{"sigma":0.9488,"successes":0,"failures":3}' \
  <<<"$OUT" || { echo "FAIL bad reputation"; exit 1; }

# All peers unavailable + self also down → route returns ABSTAIN.
grep -q '"abstain_all_down":true'                             <<<"$OUT" || { echo "FAIL abstain"; exit 1; }

echo "check-sigma-mesh: σ-routing + reputation + abstain matrix green"
