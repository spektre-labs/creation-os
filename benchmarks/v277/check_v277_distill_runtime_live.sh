#!/usr/bin/env bash
#
# v277 σ-Distill-Runtime — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * teacher/student pair = {api-claude, bitnet-3B-local}
#     and distinct
#   * 4 σ-filter rows; LEARN iff σ_teacher <=
#     τ_learn=0.25 else SKIP; both branches fire
#   * 3 domains canonical (law, code, medical);
#     LOCAL_ONLY iff σ_domain <= τ_domain=0.30 else API;
#     both branches fire
#   * 4 trajectory checkpoints (month_0..month_12):
#     shares sum to 1, api_share strictly decreasing,
#     local_share strictly increasing, cost strictly
#     decreasing, api_share[0] >= 0.75, api_share[-1]
#     <= 0.10
#   * sigma_distill in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v277_distill_runtime"
[ -x "$BIN" ] || { echo "v277: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v277", d
assert d["chain_valid"] is True, d
assert abs(d["tau_learn"]  - 0.25) < 1e-6, d
assert abs(d["tau_domain"] - 0.30) < 1e-6, d

P = d["pair"]
assert P["teacher"] == "api-claude", P
assert P["student"] == "bitnet-3B-local", P
assert P["teacher"] != P["student"], P
assert d["pair_ok"] is True, d

F = d["filter"]
assert len(F) == 4
nl = ns = 0
for f in F:
    exp = "LEARN" if f["sigma_teacher"] <= d["tau_learn"] else "SKIP"
    assert f["decision"] == exp, f
    if f["decision"] == "LEARN": nl += 1
    else:                          ns += 1
assert nl >= 1 and ns >= 1, (nl, ns)
assert d["n_filter_rows_ok"] == 4, d
assert d["n_filter_learn"]   == nl, d
assert d["n_filter_skip"]    == ns, d

DOM = d["domains"]
assert [x["name"] for x in DOM] == ["law","code","medical"], DOM
n_loc = n_api = 0
for x in DOM:
    exp = "LOCAL_ONLY" if x["sigma_domain"] <= d["tau_domain"] else "API"
    assert x["route"] == exp, x
    if x["route"] == "LOCAL_ONLY": n_loc += 1
    else:                            n_api += 1
assert n_loc >= 1 and n_api >= 1, (n_loc, n_api)
assert d["n_domain_rows_ok"] == 3, d
assert d["n_domain_local"]   == n_loc, d
assert d["n_domain_api"]     == n_api, d

T = d["trajectory"]
labels = [c["label"] for c in T]
assert labels == ["month_0","month_1","month_3","month_12"], labels
for c in T:
    assert abs(c["api_share"] + c["local_share"] - 1.0) < 1e-4, c
for i in range(1, len(T)):
    assert T[i]["api_share"]   < T[i-1]["api_share"],  (i, T[i], T[i-1])
    assert T[i]["local_share"] > T[i-1]["local_share"],(i, T[i], T[i-1])
    assert T[i]["cost_eur_per_month"] < T[i-1]["cost_eur_per_month"], (i, T[i], T[i-1])
assert T[0]["api_share"]  >= 0.75, T[0]
assert T[-1]["api_share"] <= 0.10, T[-1]
assert d["n_trajectory_rows_ok"]   == 4, d
assert d["traj_shares_ok"]         is True, d
assert d["traj_monotone_api_ok"]   is True, d
assert d["traj_monotone_local_ok"] is True, d
assert d["traj_monotone_cost_ok"]  is True, d
assert d["traj_anchors_ok"]        is True, d

assert 0.0 <= d["sigma_distill"] <= 1.0, d
assert d["sigma_distill"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v277: non-deterministic" >&2; exit 1; }
