#!/usr/bin/env bash
#
# v305 σ-Swarm-Intelligence — merge-gate check.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v305_swarm"
[ -x "$BIN" ] || { echo "v305: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v305_swarm", d
assert d["chain_valid"] is True, d

W = d["wisdom"]
want = ["best_single", "naive_average", "sigma_weighted"]
assert [r["strategy"] for r in W] == want, W
sigmas = [r["sigma"] for r in W]
accs   = [r["accuracy"] for r in W]
assert sigmas.index(min(sigmas)) == 2, W
assert accs.index(max(accs)) == 2, W
n_wins = sum(1 for r in W if r["verdict"] == "WINS")
assert n_wins == 1, W
assert W[2]["verdict"] == "WINS", W
assert d["wis_rows_ok"]           == 3,    d
assert d["wis_lowest_sigma_ok"]   is True, d
assert d["wis_highest_acc_ok"]    is True, d
assert d["wis_wins_count_ok"]     is True, d

V = d["diversity"]
want_v = ["echo_chamber", "balanced", "chaos"]
assert [r["crowd"] for r in V] == want_v, V
values = []
for r in V:
    expected = r["diversity"] * (1.0 - r["ind_sigma"])
    assert abs(r["value"] - expected) < 1e-3, r
    values.append(r["value"])
assert values.index(max(values)) == 1, V
TAU_V = d["tau_value"]
n_over = sum(1 for r in V if r["value"] > TAU_V)
assert n_over == 1, V
assert d["div_rows_ok"]         == 3,    d
assert d["div_formula_ok"]      is True, d
assert d["div_balanced_ok"]     is True, d
assert d["div_threshold_ok"]    is True, d

E = d["emergent"]
want_e = ["genuine_emergent", "weak_signal", "random_correlation"]
assert [r["pattern"] for r in E] == want_e, E
TAU_E = d["tau_emergent"]
prev = -1.0
for r in E:
    assert r["sigma_emergent"] > prev, E
    prev = r["sigma_emergent"]
    assert r["keep"] is (r["sigma_emergent"] <= TAU_E), r
n_k = sum(1 for r in E if r["keep"])
n_r = sum(1 for r in E if not r["keep"])
assert n_k >= 1 and n_r >= 1, E
assert d["em_rows_ok"]      == 3,    d
assert d["em_order_ok"]     is True, d
assert d["em_polarity_ok"]  is True, d

P = d["proconductor"]
want_p = ["claude", "gpt", "gemini", "deepseek"]
assert [r["agent"] for r in P] == want_p, P
assert len(set([r["agent"] for r in P])) == 4, P
TAU_C = d["tau_conv"]
for r in P:
    assert r["sigma"] <= TAU_C, r
    assert r["direction"] == "POSITIVE", r
assert d["pc_rows_ok"]          == 4,    d
assert d["pc_distinct_ok"]      is True, d
assert d["pc_sigma_bound_ok"]   is True, d
assert d["pc_direction_ok"]     is True, d
assert d["pc_convergent_ok"]    is True, d

assert 0.0 <= d["sigma_swarm"] <= 1.0, d
assert d["sigma_swarm"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v305: non-deterministic" >&2; exit 1; }
