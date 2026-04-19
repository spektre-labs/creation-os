#!/usr/bin/env bash
#
# v263 σ-Mesh-Engram — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 nodes A/B/C with contiguous non-overlapping
#     shards covering [0,256); shards_ok
#   * 4 lookups: served_by == expected_node,
#     lookup_ns <= 100; every node {A,B,C} appears >=1
#     as served_by; n_lookups_ok == 4,
#     n_nodes_covered == 3
#   * 4 replication rows, replicas == 3,
#     sigma_replication in [0,1]; quorum_ok matches
#     σ <= τ_quorum=0.25; >=1 true AND >=1 false;
#     n_replication_ok == 4
#   * 4 hierarchy tiers L1..L4, latency_ns strictly
#     ascending, capacity_mb strictly ascending;
#     hierarchy_ok
#   * 4 forgetting rows, action matches σ rule
#     byte-for-byte; every branch (KEEP_L1/MOVE_L2/
#     MOVE_L3/DROP) fires >=1
#   * sigma_mesh_engram in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v263_mesh_engram"
[ -x "$BIN" ] || { echo "v263: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v263", d
assert d["chain_valid"] is True, d
assert abs(d["tau_quorum"] - 0.25) < 1e-6, d

nodes = d["nodes"]
assert [n["node_id"] for n in nodes] == ["A","B","C"], nodes
assert nodes[0]["shard_lo"] == 0
for i in range(1, 3):
    assert nodes[i]["shard_lo"] == nodes[i-1]["shard_hi"], nodes
assert nodes[-1]["shard_hi"] == 256, nodes
assert d["shards_ok"] is True

lk = d["lookups"]
assert len(lk) == 4
served = set()
for l in lk:
    assert l["expected_node"] == l["served_by"], l
    assert l["lookup_ns"] <= 100, l
    assert nodes[0]["shard_lo"] <= l["hash8"] < nodes[-1]["shard_hi"], l
    served.add(l["served_by"])
assert served == {"A","B","C"}, served
assert d["n_lookups_ok"] == 4, d
assert d["n_nodes_covered"] == 3, d

rp = d["replication"]
assert len(rp) == 4
qt = qf = 0
for r in rp:
    assert r["replicas"] == 3, r
    assert 0.0 <= r["sigma_replication"] <= 1.0, r
    exp = (r["sigma_replication"] <= d["tau_quorum"])
    assert r["quorum_ok"] is exp, r
    if r["quorum_ok"]: qt += 1
    else:              qf += 1
assert qt >= 1 and qf >= 1, (qt, qf)
assert d["n_replication_ok"] == 4, d
assert d["n_quorum_true"]  == qt, d
assert d["n_quorum_false"] == qf, d

hi = d["hierarchy"]
assert [t["tier"] for t in hi] == ["L1","L2","L3","L4"], hi
for i in range(1, 4):
    assert hi[i]["latency_ns"]  > hi[i-1]["latency_ns"],  hi
    assert hi[i]["capacity_mb"] > hi[i-1]["capacity_mb"], hi
assert d["hierarchy_ok"] is True

def want(s):
    if s <= 0.20: return "KEEP_L1"
    if s <= 0.50: return "MOVE_L2"
    if s <= 0.80: return "MOVE_L3"
    return "DROP"

fg = d["forgetting"]
assert len(fg) == 4
seen = set()
for f in fg:
    assert f["action"] == want(f["sigma_relevance"]), f
    seen.add(f["action"])
assert seen == {"KEEP_L1","MOVE_L2","MOVE_L3","DROP"}, seen
assert d["n_forget_ok"] == 4, d
assert d["n_keep"] >= 1 and d["n_l2"] >= 1 and d["n_l3"] >= 1 and d["n_drop"] >= 1, d

assert 0.0 <= d["sigma_mesh_engram"] <= 1.0, d
assert d["sigma_mesh_engram"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v263: non-deterministic" >&2; exit 1; }
