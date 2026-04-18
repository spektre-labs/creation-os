#!/usr/bin/env bash
#
# v207 σ-Design — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 12 candidates, ≥ 3 on the Pareto front
#   * ≥ 1 INFEASIBLE (law / ethics / cost violation)
#   * pareto ⇒ feasible; no Pareto member is dominated by
#     any feasible candidate
#   * σ_feasibility ∈ [0,1]
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v207_design"
[ -x "$BIN" ] || { echo "v207: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v207", d
assert d["n"] == 12, d
assert d["n_pareto"]     >= 3, d
assert d["n_infeasible"] >= 1, d
assert d["chain_valid"]  is True, d

cs = d["candidates"]
feas = [c for c in cs if c["feasible"]]
pareto = [c for c in cs if c["pareto"]]
assert all(c["feasible"] for c in pareto)
for p in pareto:
    for o in feas:
        if o["id"] == p["id"]: continue
        ge = o["perf"] >= p["perf"] - 1e-6
        le_cplx = o["cplx"] <= p["cplx"] + 1e-6
        le_cost = o["cost"] <= p["cost"] + 1e-6
        strict = (o["perf"] > p["perf"] + 1e-6 or
                  o["cplx"] < p["cplx"] - 1e-6 or
                  o["cost"] < p["cost"] - 1e-6)
        dominates = ge and le_cplx and le_cost and strict
        assert not dominates, (o, p)

for c in cs:
    assert 0.0 <= c["s_f"] <= 1.0 + 1e-6, c
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v207: non-deterministic" >&2; exit 1; }
