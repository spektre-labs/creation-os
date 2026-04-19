#!/usr/bin/env bash
#
# v271 σ-Swarm-Edge — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 mesh sensors; included == (σ_local <=
#     τ_consensus=0.50); >=1 included AND >=1 excluded
#   * sigma_swarm (consensus mean over included) <
#     sigma_raw (mean over all sensors); consensus
#     strictly improves on naive mean
#   * 4 distributed-anomaly fixtures; spatial_anomaly
#     == ((σ_center − σ_neighborhood) > 0.25); >=1 true
#     AND >=1 false
#   * 3 energy tiers canonical (charged, medium, low);
#     σ_energy strictly ascending AND sample_rate_hz
#     strictly descending (energy_monotone_ok)
#   * gateway bridged_to_engine in v262 engine set
#     (bitnet-3B-local, airllm-70B-local, engram-lookup,
#     api-claude, api-gpt); swarm_size_nodes == 6;
#     gateway_ok
#   * sigma_swarm_edge in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v271_swarm_edge"
[ -x "$BIN" ] || { echo "v271: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v271", d
assert d["chain_valid"] is True, d
assert abs(d["tau_consensus"] - 0.50) < 1e-6, d

S = d["sensors"]
assert len(S) == 6
tau = d["tau_consensus"]
n_in = n_ex = 0
for x in S:
    exp = x["sigma_local"] <= tau
    assert x["included"] == exp, x
    assert 0.0 <= x["sigma_local"] <= 1.0, x
    if x["included"]: n_in += 1
    else:              n_ex += 1
assert n_in >= 1 and n_ex >= 1, (n_in, n_ex)
assert d["n_consensus_rows_ok"] == 6, d
assert d["n_included"] == n_in, d
assert d["n_excluded"] == n_ex, d
assert d["sigma_swarm"] < d["sigma_raw"], d
assert d["consensus_improves_ok"] is True, d

A = d["anomaly"]
assert len(A) == 4
n_t = n_f = 0
for a in A:
    exp = (a["sigma_center"] - a["sigma_neighborhood"]) > a["threshold"]
    assert a["spatial_anomaly"] == exp, a
    assert abs(a["threshold"] - 0.25) < 1e-6, a
    if a["spatial_anomaly"]: n_t += 1
    else:                     n_f += 1
assert n_t >= 1 and n_f >= 1, (n_t, n_f)
assert d["n_anomaly_rows_ok"] == 4, d
assert d["n_anomaly_true"]  == n_t, d
assert d["n_anomaly_false"] == n_f, d

E = d["energy"]
assert [e["tier"] for e in E] == ["charged","medium","low"], E
for i in range(1, 3):
    assert E[i]["sigma_energy"]   > E[i-1]["sigma_energy"],   E
    assert E[i]["sample_rate_hz"] < E[i-1]["sample_rate_hz"], E
assert d["n_energy_rows_ok"] == 3, d
assert d["energy_monotone_ok"] is True, d

g = d["gateway"]
v262 = {"bitnet-3B-local","airllm-70B-local","engram-lookup","api-claude","api-gpt"}
assert g["bridged_to_engine"] in v262, g
assert g["swarm_size_nodes"] == 6, g
assert g["ok"] is True, g
assert d["gateway_ok"] is True, d

assert 0.0 <= d["sigma_swarm_edge"] <= 1.0, d
assert d["sigma_swarm_edge"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v271: non-deterministic" >&2; exit 1; }
