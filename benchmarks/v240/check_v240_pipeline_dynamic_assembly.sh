#!/usr/bin/env bash
#
# v240 σ-Pipeline — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_requests == 6; tau_branch == 0.50
#   * every stage has sigma in [0, 1]; ticks strictly
#     ascending within a request
#   * sigma_pipeline == max(stage.sigma) per request
#   * clean shapes (no branch, no fusion) keep sigma_pipeline
#     <= tau_branch
#   * exactly one branched request; sigma_at_branch > tau_branch;
#     branch_to_shape == EXPLORATORY; at least 2 stages after
#     the branch point (payload + emit)
#   * exactly one fused request; final_shape == FUSED; stage
#     sequence contains at least one CODE stage and one MORAL stage
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v240_pipeline"
[ -x "$BIN" ] || { echo "v240: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v240", d
assert d["chain_valid"] is True, d
assert d["n_requests"] == 6, d
assert abs(d["tau_branch"] - 0.50) < 1e-6, d

n_branched = n_fused = 0
for r in d["requests"]:
    assert r["n_stages"] == len(r["stages"]) > 0, r
    stages = r["stages"]
    sigmas = [st["sigma"] for st in stages]
    for s in sigmas:
        assert 0.0 <= s <= 1.0, r
    for k in range(1, len(stages)):
        assert stages[k]["tick"] > stages[k-1]["tick"], r
    assert abs(r["sigma_pipeline"] - max(sigmas)) < 1e-6, r
    if r["branched"]:
        n_branched += 1
        assert r["sigma_at_branch"] > d["tau_branch"], r
        assert r["branch_to_shape"] == "EXPLORATORY", r
        post = r["n_stages"] - (r["branch_from_stage"] + 1)
        assert post >= 2, r
    elif not r["fused"]:
        assert r["sigma_pipeline"] <= d["tau_branch"] + 1e-6, r
    if r["fused"]:
        n_fused += 1
        assert r["final_shape"] == "FUSED", r
        names = [st["name"] for st in stages]
        assert any(n in ("code_plan", "sandbox")
                   for n in names), r
        assert any(n in ("moral_analyse", "moral_verify")
                   for n in names), r

assert n_branched == 1 == d["n_branched"], d
assert n_fused    == 1 == d["n_fused"],    d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v240: non-deterministic" >&2; exit 1; }
