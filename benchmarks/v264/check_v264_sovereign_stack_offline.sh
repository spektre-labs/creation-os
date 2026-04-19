#!/usr/bin/env bash
#
# v264 σ-Sovereign-Stack — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 7 stack layers in canonical order
#     (hardware, model, memory, gate, network,
#      api_fallback, license); open_source for all;
#     layer api_fallback is the ONLY
#     requires_cloud==true AND the ONLY
#     works_offline==false; every other layer has
#     works_offline==true AND requires_cloud==false;
#     n_layers_ok == 7
#   * 4 offline flows; used_cloud==false, ok==true;
#     engine in {bitnet-3B-local, airllm-70B-local,
#     engram-lookup}; >=2 distinct local engines;
#     n_flows_ok == 4; n_distinct_local_engines >= 2
#   * 4 sovereign anchors (v234, v182, v148, v238),
#     all fulfilled; n_anchors_ok == 4
#   * cost: eur_baseline==200, eur_sigma_sovereign==20,
#     reduction_pct==90, cost_ok
#   * sigma_sovereign_stack in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v264_sovereign_stack"
[ -x "$BIN" ] || { echo "v264: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v264", d
assert d["chain_valid"] is True, d

ln = [l["name"] for l in d["layers"]]
assert ln == ["hardware","model","memory","gate","network",
              "api_fallback","license"], ln
for i, l in enumerate(d["layers"]):
    assert l["open_source"] is True, l
    if l["name"] == "api_fallback":
        assert l["works_offline"]  is False, l
        assert l["requires_cloud"] is True,  l
    else:
        assert l["works_offline"]  is True,  l
        assert l["requires_cloud"] is False, l
assert d["n_layers_ok"] == 7, d

local = {"bitnet-3B-local","airllm-70B-local","engram-lookup"}
eng = set()
for f in d["flows"]:
    assert f["used_cloud"] is False, f
    assert f["ok"] is True, f
    assert f["engine"] in local, f
    eng.add(f["engine"])
assert len(eng) >= 2, eng
assert d["n_flows_ok"] == 4, d
assert d["n_distinct_local_engines"] == len(eng), d

ak = [a["kernel"] for a in d["anchors"]]
assert ak == ["v234","v182","v148","v238"], ak
for a in d["anchors"]:
    assert a["fulfilled"] is True, a
assert d["n_anchors_ok"] == 4, d

c = d["cost"]
assert c["eur_baseline"] == 200, c
assert c["eur_sigma_sovereign"] == 20, c
assert c["reduction_pct"] == 90, c
assert c["cost_ok"] is True, c

assert 0.0 <= d["sigma_sovereign_stack"] <= 1.0, d
assert d["sigma_sovereign_stack"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v264: non-deterministic" >&2; exit 1; }
