#!/usr/bin/env bash
#
# v262 σ-Hybrid-Engine — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 5 engines in canonical order:
#       bitnet-3B-local, airllm-70B-local, engram-lookup,
#       api-claude, api-gpt
#     cost_per_1k_eur >= 0, sigma_floor in [0,1],
#     engine_ok; n_engines_ok == 5
#   * 4 routing fixtures: sigma_difficulty in [0,1];
#     chosen_engine in the registry; >=3 DISTINCT
#     engines chosen; n_routes_ok == 4
#   * 4 cascade steps: decision matches σ vs
#     τ_accept=0.40 (≤→OK else ESCALATE); step 0 MUST
#     be ESCALATE; >=1 OK AND >=1 ESCALATE AND >=1
#     cloud-tier step
#   * cost: local_pct + api_pct == 100, local_pct >= 80,
#     savings_pct matches 100*(api_only - sigma_route)/
#     api_only within ±1 pt, savings_pct >= 80, cost_ok
#   * sigma_hybrid_engine in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v262_hybrid_engine"
[ -x "$BIN" ] || { echo "v262: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v262", d
assert d["chain_valid"] is True, d
assert abs(d["tau_accept"] - 0.40) < 1e-6, d

en = [e["name"] for e in d["engines"]]
assert en == ["bitnet-3B-local","airllm-70B-local","engram-lookup",
              "api-claude","api-gpt"], en
cloud = {e["name"] for e in d["engines"] if e["tier"] == "cloud"}
for e in d["engines"]:
    assert e["cost_per_1k_eur"] >= 0.0, e
    assert 0.0 <= e["sigma_floor"] <= 1.0, e
    assert e["engine_ok"] is True, e
assert d["n_engines_ok"] == 5, d

registry = set(en)
chosen = set()
for r in d["routes"]:
    assert 0.0 <= r["sigma_difficulty"] <= 1.0, r
    assert r["chosen_engine"] in registry, r
    chosen.add(r["chosen_engine"])
assert len(chosen) >= 3, chosen
assert d["n_routes_ok"] == 4, d
assert d["n_distinct_engines"] == len(chosen), d

tau = d["tau_accept"]
ok = esc = 0
cloud_used = 0
for i, c in enumerate(d["cascade"]):
    exp = "OK" if c["sigma_result"] <= tau else "ESCALATE"
    assert c["decision"] == exp, c
    if c["decision"] == "OK":       ok  += 1
    if c["decision"] == "ESCALATE": esc += 1
    if c["engine"] in cloud:        cloud_used += 1
assert d["cascade"][0]["decision"] == "ESCALATE", d["cascade"][0]
assert ok >= 1 and esc >= 1 and cloud_used >= 1, (ok, esc, cloud_used)
assert d["n_escalate"]   == esc, d
assert d["n_cloud_used"] == cloud_used, d

co = d["cost"]
assert co["local_pct"] + co["api_pct"] == 100, co
assert co["local_pct"] >= 80, co
expect = 100.0 * (co["eur_api_only"] - co["eur_sigma_route"]) / co["eur_api_only"]
assert abs(co["savings_pct"] - expect) <= 1.0, (co, expect)
assert co["savings_pct"] >= 80, co
assert co["cost_ok"] is True, co

assert 0.0 <= d["sigma_hybrid_engine"] <= 1.0, d
assert d["sigma_hybrid_engine"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v262: non-deterministic" >&2; exit 1; }
