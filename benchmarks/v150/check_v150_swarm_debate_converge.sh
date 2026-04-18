#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v150 merge-gate: σ-swarm debate converges + adversarial verify + routing.
set -euo pipefail

BIN=./creation_os_v150_swarm
[[ -x "$BIN" ]] || { echo "missing binary $BIN"; exit 1; }

# -----------------------------------------------------------------
# T1: self-test passes.
# -----------------------------------------------------------------
out=$("$BIN" --self-test)
echo "  self-test: $out"
echo "$out" | grep -q '"self_test":"pass"' || { echo "FAIL: self-test"; exit 1; }

# -----------------------------------------------------------------
# T2: math debate converges (σ_consensus < 0.5) and best = 0.
# -----------------------------------------------------------------
math=$("$BIN" --debate math --question "solve 2x+3=7" --seed 1500)
echo "  math debate: $math" | head -c 300
converged=$(echo "$math" | sed -E 's/.*"converged":(true|false).*/\1/')
best=$(echo "$math"      | sed -E 's/.*"best":([0-9]+).*/\1/')
[[ "$converged" == "true" ]] || { echo "FAIL: math debate did not converge"; exit 1; }
[[ "$best" == "0" ]]         || { echo "FAIL: math should route to atlas(id=0), got $best"; exit 1; }
echo

# -----------------------------------------------------------------
# T3: σ_collective < σ of worst-specialist round-0 answer
#     (the √N shrinkage must be visible).
# -----------------------------------------------------------------
sc=$(echo "$math" | sed -E 's/.*"sigma_collective":([0-9.]+).*/\1/')
awk -v s="$sc" 'BEGIN{exit !(s+0 > 0 && s+0 < 0.60)}' \
    || { echo "FAIL: σ_collective=$sc outside (0, 0.60)"; exit 1; }

# -----------------------------------------------------------------
# T4: adversarial verify emits N-1 critiques and changes σ.
# -----------------------------------------------------------------
verify=$("$BIN" --debate math --question "solve 2x+3=7" --seed 1500 --verify)
n_critiques=$(echo "$verify" | grep -o '"sigma_crit"' | wc -l | tr -d ' ')
[[ "$n_critiques" == "2" ]] || { echo "FAIL: expected 2 critiques, got $n_critiques"; exit 1; }

# -----------------------------------------------------------------
# T5: routing picks the right specialist per topic tag.
# -----------------------------------------------------------------
code=$("$BIN" --route code --seed 1500)
[[ $(echo "$code" | sed -E 's/.*"specialist_id":([0-9]+).*/\1/') == "1" ]] \
    || { echo "FAIL: code should route to crow(id=1): $code"; exit 1; }
bio=$("$BIN" --route bio --seed 1500)
[[ $(echo "$bio"  | sed -E 's/.*"specialist_id":([0-9]+).*/\1/') == "2" ]] \
    || { echo "FAIL: bio should route to lynx(id=2): $bio"; exit 1; }

# -----------------------------------------------------------------
# T6: determinism — same seed, same bytes.
# -----------------------------------------------------------------
a=$("$BIN" --debate code --question "implement quicksort" --seed 42)
b=$("$BIN" --debate code --question "implement quicksort" --seed 42)
[[ "$a" == "$b" ]] || { echo "FAIL: v150 not deterministic"; exit 1; }

echo "v150 swarm debate smoke: OK"
