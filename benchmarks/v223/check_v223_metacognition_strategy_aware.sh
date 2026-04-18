#!/usr/bin/env bash
#
# v223 σ-Meta-Cognition — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 paths, 5 strategies, 4 problem types, 3 biases
#   * tool/task fit priors in sigma_strategy respected
#   * strategy-aware: σ_choice == prior matrix entry
#   * ≥ 1 bias flag ≥ 0.30; ≥ 1 σ_goedel ≥ 0.80
#   * n_metacognitive ≥ 5
#   * σ_total ∈ [0, 1]
#   * weights sum to 1
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v223_meta_cognition"
[ -x "$BIN" ] || { echo "v223: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v223", d
assert d["n_paths"] == 6 and d["n_strats"] == 5, d
assert d["n_probtypes"] == 4 and d["n_biases"] == 3, d
assert d["chain_valid"] is True, d

w = d["weights"]
assert abs(sum(w) - 1.0) < 1e-6, w

sigma_strategy = d["sigma_strategy"]
# logic × deduction ≤ 0.15
assert sigma_strategy[0][0] <= 0.15 + 1e-6, sigma_strategy
# logic × heuristic ≥ 0.50
assert sigma_strategy[0][4] >= 0.50 - 1e-6, sigma_strategy
# creative × deduction ≥ 0.60
assert sigma_strategy[2][0] >= 0.60 - 1e-6, sigma_strategy
# creative × analogy ≤ 0.25
assert sigma_strategy[2][2] <= 0.25 + 1e-6, sigma_strategy

for p in d["paths"]:
    expected = sigma_strategy[p["pt"]][p["strat"]]
    assert abs(p["sigma_choice"] - expected) < 1e-6, p
    assert 0.0 <= p["sigma_total"]  <= 1.0 + 1e-6, p
    assert 0.10 - 1e-6 <= p["sigma_goedel"] <= 1.0 + 1e-6, p
    assert 0.0 <= p["sigma_meta"]   <= 1.0 + 1e-6, p
    assert 0.0 <= p["sigma_bias"]   <= 1.0 + 1e-6, p

assert d["n_metacognitive"] >= 5, d
assert d["n_bias_flags_hi"] >= 1, d
assert d["n_goedel_hi"]     >= 1, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v223: non-deterministic" >&2; exit 1; }
