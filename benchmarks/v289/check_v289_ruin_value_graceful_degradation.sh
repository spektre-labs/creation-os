#!/usr/bin/env bash
#
# v289 σ-Ruin-Value — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 removal rows canonical (v267_mamba→
#     transformer, v260_engram→local_memory,
#     v275_ttt→frozen_weights, v262_hybrid→
#     direct_kernel); every survivor_still_works;
#     all distinct
#   * 4 cascade tiers canonical (hybrid_engine /
#     transformer_only / bitnet_plus_sigma /
#     pure_sigma_gate); tier_id permutation [1..4];
#     all standalone_viable; resource_cost strictly
#     monotonically decreasing
#   * 3 preservation rows canonical
#     (sigma_log_persisted, atomic_write_wal,
#     last_measurement_recoverable) all guaranteed
#   * 3 rebuild steps canonical (read_sigma_log→
#     restore_last_state→resume_not_restart);
#     step_order permutation [1, 2, 3]; all possible
#   * seed_kernels_required == 5
#   * sigma_ruin in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v289_ruin_value"
[ -x "$BIN" ] || { echo "v289: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v289", d
assert d["chain_valid"] is True, d

R = d["removal"]
want_r = [
    ("v267_mamba",  "transformer"),
    ("v260_engram", "local_memory"),
    ("v275_ttt",    "frozen_weights"),
    ("v262_hybrid", "direct_kernel"),
]
for (k, sv), r in zip(want_r, R):
    assert r["kernel_removed"] == k, r
    assert r["survivor"]       == sv, r
    assert r["survivor_still_works"] is True, r
assert len({r["kernel_removed"] for r in R}) == 4, R
assert d["n_removal_rows_ok"]      == 4, d
assert d["removal_all_survive_ok"] is True, d
assert d["removal_distinct_ok"]    is True, d

C = d["cascade"]
want_c = [
    (1, "hybrid_engine",       1.00),
    (2, "transformer_only",    0.60),
    (3, "bitnet_plus_sigma",   0.25),
    (4, "pure_sigma_gate",     0.05),
]
for (tid, n, cost), r in zip(want_c, C):
    assert r["tier_id"]       == tid, r
    assert r["name"]          == n, r
    assert abs(r["resource_cost"] - cost) < 1e-5, r
    assert r["standalone_viable"] is True, r
assert sorted({r["tier_id"] for r in C}) == [1,2,3,4], C
costs = [r["resource_cost"] for r in C]
assert all(costs[i] > costs[i+1] for i in range(len(costs)-1)), costs
assert d["n_cascade_rows_ok"]             == 4, d
assert d["cascade_tier_permutation_ok"]   is True, d
assert d["cascade_all_viable_ok"]         is True, d
assert d["cascade_cost_monotone_ok"]      is True, d

P = d["preserve"]
want_p = ["sigma_log_persisted", "atomic_write_wal",
          "last_measurement_recoverable"]
assert [r["name"] for r in P] == want_p, P
for r in P:
    assert r["guaranteed"] is True, r
assert d["n_preserve_rows_ok"]         == 3, d
assert d["preserve_all_guaranteed_ok"] is True, d

B = d["rebuild"]
want_b = [("read_sigma_log",1), ("restore_last_state",2),
          ("resume_not_restart",3)]
for (n, o), r in zip(want_b, B):
    assert r["step_name"]  == n, r
    assert r["step_order"] == o, r
    assert r["possible"]   is True, r
assert sorted({r["step_order"] for r in B}) == [1,2,3], B
assert d["n_rebuild_rows_ok"]            == 3, d
assert d["rebuild_order_permutation_ok"] is True, d
assert d["rebuild_all_possible_ok"]      is True, d

assert d["seed_kernels_required"] == 5, d
assert d["seed_required_ok"]      is True, d

assert 0.0 <= d["sigma_ruin"] <= 1.0, d
assert d["sigma_ruin"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v289: non-deterministic" >&2; exit 1; }
