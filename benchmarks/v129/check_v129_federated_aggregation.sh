#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Merge-gate for v129 σ-Federated.
#
# Validates:
#   1. self-test green (σ-FedAvg, DP noise, top-K, unlearn)
#   2. aggregate CLI produces sum-to-1 weights and picks low-σ winner
#   3. σ-scaled DP: σ_dp(σ=0.9) > σ_dp(σ=0.1) at same ε
#   4. σ-adaptive K: confident → more coords shared than uncertain
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BIN="$ROOT/creation_os_v129_federated"
if [ ! -x "$BIN" ]; then
    echo "check-v129: building creation_os_v129_federated..." >&2
    make -s creation_os_v129_federated
fi

echo "check-v129-federated-aggregation: --self-test"
"$BIN" --self-test >/dev/null

echo "check-v129-federated-aggregation: σ-weighted aggregate 3 nodes"
OUT="$("$BIN" --aggregate "0.10,0.50,0.90" "1.0,2.0,3.0,4.0")"
echo "  agg-stats: ${OUT%%$'\n'*}"
echo "$OUT" | grep -q '"n_nodes":3'                  || { echo "FAIL n_nodes"; exit 1; }
WINNER=$(echo "$OUT" | head -1 | sed -n 's/.*"winner_node_id":\([0-9-]*\).*/\1/p')
[ "$WINNER" = "0" ] || { echo "FAIL winner_node_id=$WINNER (expected 0)"; exit 1; }
WSUM=$(echo "$OUT" | head -1 | sed -n 's/.*"weight_sum":\([0-9.]*\).*/\1/p')
python3 - <<EOF
ws = float("$WSUM")
assert abs(ws - 1.0) < 1e-4, f"weight_sum={ws} (expected 1.0)"
print(f"  weight_sum ok: {ws:.6f}")
EOF

echo "check-v129-federated-aggregation: DP noise scales with σ_node"
LO="$("$BIN" --dp-noise 0.10 1.0)"
HI="$("$BIN" --dp-noise 0.90 1.0)"
echo "  σ_node=0.1: $LO"
echo "  σ_node=0.9: $HI"
python3 - <<EOF
import re
lo = float(re.search(r'"sigma_dp":([0-9.eE+-]+)', """$LO""").group(1))
hi = float(re.search(r'"sigma_dp":([0-9.eE+-]+)', """$HI""").group(1))
assert hi > lo, f"σ_dp high={hi} must exceed low={lo}"
print(f"  σ_dp(0.9)={hi:.6f} > σ_dp(0.1)={lo:.6f} ok")
EOF

echo "check-v129-federated-aggregation: adaptive-K — confident > uncertain"
CONF="$("$BIN" --adaptive-k 0.10)"
UNC="$("$BIN" --adaptive-k 0.90)"
echo "  confident: $CONF"
echo "  uncertain: $UNC"
python3 - <<EOF
import re
kc = int(re.search(r'"k":(\d+)', """$CONF""").group(1))
ku = int(re.search(r'"k":(\d+)', """$UNC""").group(1))
assert kc > ku, f"k confident {kc} must exceed k uncertain {ku}"
assert kc >= 32,   f"confident k={kc} must be ≥ 32"
assert ku <= 16,   f"uncertain k={ku} must be ≤ 16"
print(f"  K_confident={kc} > K_uncertain={ku} ok")
EOF

echo "check-v129-federated-aggregation: OK"
