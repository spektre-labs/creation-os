#!/usr/bin/env bash
#
# v253 σ-Ecosystem-Hub — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * hub_url == "hub.creation-os.dev"
#   * exactly 5 hub sections in canonical order
#     (marketplace, agent_directory, documentation,
#      community_forum, benchmark_dashboard), every
#     upstream non-empty, every section_ok
#   * exactly 4 health metrics in canonical order
#     (active_instances, kernels_published,
#      a2a_federations, contributors_30d), every
#     value > 0
#   * exactly 5 contribution steps in canonical order
#     (fork, write_kernel, pull_request, merge_gate,
#      publish), every step_ok
#   * 4 roadmap proposals; >=1 with votes > 0;
#     exactly 1 with proconductor_decision == true
#   * exactly 4 unity assertions in canonical order
#     (instance, kernel, interaction, a2a_message),
#     every declared AND realized
#   * sigma_ecosystem in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v253_ecosystem_hub"
[ -x "$BIN" ] || { echo "v253: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v253", d
assert d["chain_valid"] is True, d
assert d["hub_url"] == "hub.creation-os.dev", d

sn = [s["name"] for s in d["sections"]]
assert sn == ["marketplace","agent_directory","documentation",
              "community_forum","benchmark_dashboard"], sn
for s in d["sections"]:
    assert len(s["upstream"]) > 0, s
    assert s["section_ok"] is True, s
assert d["n_sections_ok"] == 5, d

hn = [m["name"] for m in d["health"]]
assert hn == ["active_instances","kernels_published",
              "a2a_federations","contributors_30d"], hn
for m in d["health"]:
    assert m["value"] > 0, m
assert d["n_health_positive"] == 4, d

cn = [c["step"] for c in d["contrib"]]
assert cn == ["fork","write_kernel","pull_request",
              "merge_gate","publish"], cn
for c in d["contrib"]:
    assert c["step_ok"] is True, c
assert d["n_contrib_ok"] == 5, d

assert d["n_roadmap_voted_in"]        >= 1, d
assert d["n_proconductor_decisions"]  == 1, d
pc_flags = [p["proconductor_decision"] for p in d["roadmap"]]
assert sum(1 for v in pc_flags if v is True) == 1, pc_flags

un = [u["scope"] for u in d["unity"]]
assert un == ["instance","kernel","interaction","a2a_message"], un
for u in d["unity"]:
    assert u["declared"] is True, u
    assert u["realized"] is True, u
assert d["n_unity_ok"] == 4, d

assert 0.0 <= d["sigma_ecosystem"] <= 1.0, d
assert d["sigma_ecosystem"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v253: non-deterministic" >&2; exit 1; }
