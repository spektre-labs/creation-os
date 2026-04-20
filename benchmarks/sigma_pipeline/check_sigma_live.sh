#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_live
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0'   <<<"$OUT" || { echo "rc != 0"        >&2; exit 3; }
grep -q '"pass":true'        <<<"$OUT" || { echo "pass != true"   >&2; exit 4; }

# 10 observations + mixed curve described in live.c:
#   σ_accept settles to 0.2500 (last bin where cumulative acc ≥ 0.95)
#   σ_rethink settles to 0.7500 (last bin where cumulative acc ≥ 0.50)
#   lifetime accuracy = 5/10 = 0.5000
grep -q '"n_obs":10'                   <<<"$OUT" || { echo "n_obs != 10"    >&2; exit 5; }
grep -q '"tau_accept":0.2500'          <<<"$OUT" || { echo "tau_accept"     >&2; exit 6; }
grep -q '"tau_rethink":0.7500'         <<<"$OUT" || { echo "tau_rethink"    >&2; exit 7; }
grep -q '"lifetime_accuracy":0.5000'   <<<"$OUT" || { echo "lifetime"       >&2; exit 8; }
grep -q '"seed_tau_accept":0.3000'     <<<"$OUT" || { echo "seed_accept"    >&2; exit 9; }
grep -q '"seed_tau_rethink":0.6000'    <<<"$OUT" || { echo "seed_rethink"   >&2; exit 10; }

# After every observation we re-adapt, so n_adapts == n_obs == 10.
grep -q '"n_adapts":10'                <<<"$OUT" || { echo "n_adapts != 10" >&2; exit 11; }

echo "check-sigma-live: PASS (τ_accept 0.30→0.25, τ_rethink 0.60→0.75, lifetime 50%, 10 adapts)"
