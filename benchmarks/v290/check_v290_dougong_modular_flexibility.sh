#!/usr/bin/env bash
#
# v290 σ-Dougong — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 coupling rows canonical (v267_mamba→
#     v262_hybrid, v260_engram→v206_ghosts,
#     v275_ttt→v272_agentic_rl, v286_interp→
#     v269_stopping); every channel==
#     sigma_measurement_t; no row direct_call
#   * 3 hot-swap rows canonical (v267_mamba→
#     v276_deltanet, v216_quorum→v214_swarm,
#     v232_sqlite→v224_snapshot); every row
#     downtime_ms==0 AND config_unchanged
#   * 3 seismic rows canonical (spike_small/
#     spike_medium/spike_large); every row
#     distributed AND max_sigma_load<=load_budget
#     (0.80)
#   * 3 chaos rows canonical (kill_random_kernel,
#     overload_single_kernel, network_partition)
#     with distinct outcomes; all passed
#   * sigma_dougong in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v290_dougong"
[ -x "$BIN" ] || { echo "v290: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v290", d
assert d["chain_valid"] is True, d
assert abs(d["load_budget"] - 0.80) < 1e-6, d

C = d["coupling"]
want_c = [
    ("v267_mamba",  "v262_hybrid"),
    ("v260_engram", "v206_ghosts"),
    ("v275_ttt",    "v272_agentic_rl"),
    ("v286_interp", "v269_stopping"),
]
for (p, cons), r in zip(want_c, C):
    assert r["producer"] == p, r
    assert r["consumer"] == cons, r
    assert r["channel"]  == "sigma_measurement_t", r
    assert r["direct_call"] is False, r
assert d["n_coupling_rows_ok"]            == 4, d
assert d["coupling_channel_ok"]           is True, d
assert d["coupling_no_direct_call_ok"]    is True, d

W = d["swap"]
want_w = [
    ("v267_mamba",   "v276_deltanet", "long_context"),
    ("v216_quorum",  "v214_swarm",    "agent_consensus"),
    ("v232_sqlite",  "v224_snapshot", "state_persistence"),
]
for (f, t, l), r in zip(want_w, W):
    assert r["from_kernel"]      == f, r
    assert r["to_kernel"]        == t, r
    assert r["layer"]            == l, r
    assert r["downtime_ms"]      == 0, r
    assert r["config_unchanged"] is True, r
assert d["n_swap_rows_ok"]          == 3, d
assert d["swap_zero_downtime_ok"]   is True, d
assert d["swap_config_unchanged_ok"] is True, d

Q = d["seismic"]
want_q = [("spike_small",0.40), ("spike_medium",0.60), ("spike_large",0.78)]
for (n, l), r in zip(want_q, Q):
    assert r["name"] == n, r
    assert abs(r["max_sigma_load"] - l) < 1e-5, r
    assert r["load_distributed"] is True, r
    assert r["max_sigma_load"] <= d["load_budget"] + 1e-9, r
assert d["n_seismic_rows_ok"]       == 3, d
assert d["seismic_distributed_ok"]  is True, d
assert d["seismic_load_bounded_ok"] is True, d

X = d["chaos"]
want_x = [
    ("kill_random_kernel",     "survived"),
    ("overload_single_kernel", "load_distributed"),
    ("network_partition",      "degraded_but_alive"),
]
for (s, o), r in zip(want_x, X):
    assert r["scenario"] == s, r
    assert r["outcome"]  == o, r
    assert r["passed"]   is True, r
assert len({r["outcome"] for r in X}) == 3, X
assert d["n_chaos_rows_ok"]           == 3, d
assert d["chaos_outcome_distinct_ok"] is True, d
assert d["chaos_all_passed_ok"]       is True, d

assert 0.0 <= d["sigma_dougong"] <= 1.0, d
assert d["sigma_dougong"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v290: non-deterministic" >&2; exit 1; }
