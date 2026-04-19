#!/usr/bin/env bash
#
# v249 σ-MCP — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * jsonrpc_version == "2.0"
#   * exactly 5 tools in order (reason, plan, create,
#     simulate, teach), every tool_ok
#   * exactly 3 resources in order (memory, ontology, skills),
#     every resource_ok
#   * exactly 4 external servers in order (database, api,
#     filesystem, search), every sigma_trust in [0,1]
#   * exactly 5 tool calls; σ-gating has teeth:
#     >=3 USE, >=1 WARN (σ > τ_tool=0.40), >=1 REFUSE
#     (σ > τ_refuse=0.75); decision matches thresholds
#   * exactly 3 discovery modes (local, mdns, registry),
#     every mode_ok; ontology_mapping_ok
#   * sigma_mcp in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v249_mcp"
[ -x "$BIN" ] || { echo "v249: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v249", d
assert d["chain_valid"] is True, d
assert d["jsonrpc_version"] == "2.0", d

tnames = [t["name"] for t in d["tools"]]
assert tnames == ["reason","plan","create","simulate","teach"], tnames
for t in d["tools"]:
    assert t["tool_ok"] is True, t
assert d["n_tools_ok"] == 5, d

rnames = [r["name"] for r in d["resources"]]
assert rnames == ["memory","ontology","skills"], rnames
for r in d["resources"]:
    assert r["resource_ok"] is True, r
assert d["n_resources_ok"] == 3, d

enames = [e["name"] for e in d["externals"]]
assert enames == ["database","api","filesystem","search"], enames
for e in d["externals"]:
    assert 0.0 <= e["sigma_trust"] <= 1.0, e
assert d["n_externals_ok"] == 4, d

tt   = d["tau_tool"]
trf  = d["tau_refuse"]
assert abs(tt  - 0.40) < 1e-6, d
assert abs(trf - 0.75) < 1e-6, d

assert d["n_use"]    >= 3, d
assert d["n_warn"]   >= 1, d
assert d["n_refuse"] >= 1, d
assert d["n_use"] + d["n_warn"] + d["n_refuse"] == 5, d
for c in d["calls"]:
    assert 0.0 <= c["sigma_result"] <= 1.0, c
    if   c["sigma_result"] > trf: exp = "REFUSE"
    elif c["sigma_result"] > tt : exp = "WARN"
    else:                          exp = "USE"
    assert c["decision"] == exp, c

dmodes = [x["mode"] for x in d["discovery"]]
assert dmodes == ["local","mdns","registry"], dmodes
for x in d["discovery"]:
    assert x["mode_ok"] is True, x
assert d["ontology_mapping_ok"] is True, d

assert 0.0 <= d["sigma_mcp"] <= 1.0, d
assert d["sigma_mcp"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v249: non-deterministic" >&2; exit 1; }
