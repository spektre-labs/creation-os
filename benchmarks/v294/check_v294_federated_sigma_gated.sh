#!/usr/bin/env bash
#
# v294 σ-Federated — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 canonical devices (device_a/b/c); accept iff
#     σ_device ≤ τ_device=0.40 (both branches fire);
#     weights of ACCEPTED devices sum to 1.0 ± 1e-3;
#     weights strictly decreasing with σ across
#     ACCEPTED rows (low σ pulls more aggregate)
#   * 3 DP regimes (too_low_noise/optimal_noise/
#     too_high_noise); σ strictly increasing as ε
#     strictly decreasing; exactly 1 OPTIMAL
#   * 3 non-IID rows (similar_data/slightly_different/
#     very_different); σ_dist strictly increasing;
#     GLOBAL_MODEL iff σ_dist < 0.20, PERSONALIZED iff
#     σ_dist > 0.60, HYBRID otherwise — all three
#     branches fire
#   * 3 mesh edges (a->b/b->c/a->z); trusted iff
#     σ_neighbor ≤ τ_mesh=0.30 (both branches fire);
#     central_server=false; no single point of failure
#   * sigma_fed in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v294_federated"
[ -x "$BIN" ] || { echo "v294: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v294", d
assert d["chain_valid"] is True, d

DV = d["dev"]
want_id = ["device_a","device_b","device_c"]
assert [r["id"] for r in DV] == want_id, DV
tau_device = 0.40
accepted_rows = []
for r in DV:
    want = r["sigma_device"] <= tau_device + 1e-6
    assert r["accepted"] is want, r
    if r["accepted"]:
        accepted_rows.append(r)
# both branches fire
assert any(r["accepted"] for r in DV), DV
assert any(not r["accepted"] for r in DV), DV
# accepted weights sum to 1.0
s = sum(r["weight"] for r in accepted_rows)
assert abs(s - 1.0) < 1e-3, s
# rejected weights are zero
for r in DV:
    if not r["accepted"]:
        assert abs(r["weight"]) < 1e-6, r
# weights strictly decreasing with σ among accepted
prev_s, prev_w = -1.0, 2.0
for r in accepted_rows:
    assert r["sigma_device"] > prev_s, r
    assert r["weight"]       < prev_w, r
    prev_s, prev_w = r["sigma_device"], r["weight"]
assert d["n_dev_rows_ok"]          == 3, d
assert d["dev_accept_polarity_ok"] is True, d
assert d["dev_weight_sum_ok"]      is True, d
assert d["dev_weight_mono_ok"]     is True, d

DP = d["dp"]
want_reg = ["too_low_noise","optimal_noise","too_high_noise"]
assert [r["regime"] for r in DP] == want_reg, DP
prev_eps, prev_sig = 1e9, -1.0
n_optimal = 0
for r in DP:
    assert r["dp_epsilon"]        < prev_eps, r
    assert r["sigma_after_noise"] > prev_sig, r
    prev_eps, prev_sig = r["dp_epsilon"], r["sigma_after_noise"]
    if r["classification"] == "OPTIMAL":
        n_optimal += 1
assert n_optimal == 1, DP
assert d["n_dp_rows_ok"]    == 3, d
assert d["dp_order_ok"]     is True, d
assert d["dp_optimal_ok"]   is True, d

NI = d["niid"]
want_ty = ["similar_data","slightly_different","very_different"]
assert [r["device_type"] for r in NI] == want_ty, NI
prev_sd = -1.0
strategies = set()
for r in NI:
    assert r["sigma_distribution"] > prev_sd, r
    prev_sd = r["sigma_distribution"]
    if   r["sigma_distribution"] < 0.20 - 1e-6:
        want = "GLOBAL_MODEL"
    elif r["sigma_distribution"] > 0.60 + 1e-6:
        want = "PERSONALIZED"
    else:
        want = "HYBRID"
    assert r["strategy"] == want, r
    strategies.add(r["strategy"])
assert strategies == {"GLOBAL_MODEL","HYBRID","PERSONALIZED"}, strategies
assert d["n_niid_rows_ok"]     == 3, d
assert d["niid_order_ok"]      is True, d
assert d["niid_classify_ok"]   is True, d

ME = d["mesh"]
want_ed = ["a->b","b->c","a->z"]
assert [r["edge"] for r in ME] == want_ed, ME
tau_mesh = 0.30
for r in ME:
    want = r["sigma_neighbor"] <= tau_mesh + 1e-6
    assert r["trusted"] is want, r
assert any(r["trusted"] for r in ME), ME
assert any(not r["trusted"] for r in ME), ME
assert d["n_mesh_rows_ok"]     == 3, d
assert d["mesh_polarity_ok"]   is True, d
assert d["mesh_no_server_ok"]  is True, d

assert 0.0 <= d["sigma_fed"] <= 1.0, d
assert d["sigma_fed"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v294: non-deterministic" >&2; exit 1; }
