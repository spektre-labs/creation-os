#!/usr/bin/env bash
# σ-Evolution smoke test (S4): genome clamp + σ-gated GA over pipeline params.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_evolution
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Deterministic demo: 6-member pop, 8 generations, RNG seed 42.
grep -q '"pop_size":6'           <<<"$OUT" || { echo "pop_size"    >&2; exit 5; }
grep -q '"generations":8'        <<<"$OUT" || { echo "generations" >&2; exit 6; }
grep -q '"rng_seed":42'          <<<"$OUT" || { echo "rng_seed"    >&2; exit 7; }

# Seed genome is deliberately bad; fitness pinned.
grep -q '"seed_fitness":137.2940'    <<<"$OUT" || { echo "seed fitness" >&2; exit 8; }
# Best after 8 generations (LCG is pinned → numbers are stable).
grep -q '"best_fitness":257.3069'    <<<"$OUT" || { echo "best fitness" >&2; exit 9; }
grep -q '"best_sigma_bench":0.1757'  <<<"$OUT" || { echo "best sigma"   >&2; exit 10; }
grep -q '"improved":true'            <<<"$OUT" || { echo "improved"      >&2; exit 11; }

# τ_accept was seeded at 0.50; optimum is 0.80; convergence slack ±0.30.
grep -q '"tau_accept_near_optimum":true' <<<"$OUT" || { echo "tau_accept" >&2; exit 12; }

echo "check-sigma-evolution: PASS (genome clamp + fitness ordering + seed improvement + τ_accept convergence)"
