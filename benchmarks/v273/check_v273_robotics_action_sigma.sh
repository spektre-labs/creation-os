#!/usr/bin/env bash
#
# v273 σ-Robotics — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 action rows; decision cascade (σ<=0.20 EXECUTE,
#     σ<=0.50 SIMPLIFY, else ASK_HUMAN); >=1 of each
#   * 3 perception sensors canonical (camera, lidar,
#     ultrasonic); fused_in == (σ_local <= 0.40); both
#     fused AND excluded; sigma_percep_fused <
#     sigma_percep_naive
#   * 4 safety rows; σ_safety strictly ascending AND
#     slow_factor strictly descending (safety_monotone)
#   * 3 failure rows; σ_current > σ_prior for all
#     (all learned)
#   * sigma_robotics in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v273_robotics"
[ -x "$BIN" ] || { echo "v273: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v273", d
assert d["chain_valid"] is True, d
assert abs(d["tau_exec"]   - 0.20) < 1e-6, d
assert abs(d["tau_simple"] - 0.50) < 1e-6, d
assert abs(d["tau_fuse"]   - 0.40) < 1e-6, d

A = d["action"]
assert len(A) == 4
n_e = n_s = n_h = 0
for a in A:
    if a["sigma_action"] <= d["tau_exec"]:
        exp = "EXECUTE"
    elif a["sigma_action"] <= d["tau_simple"]:
        exp = "SIMPLIFY"
    else:
        exp = "ASK_HUMAN"
    assert a["decision"] == exp, a
    if a["decision"] == "EXECUTE":   n_e += 1
    if a["decision"] == "SIMPLIFY":  n_s += 1
    if a["decision"] == "ASK_HUMAN": n_h += 1
assert n_e >= 1 and n_s >= 1 and n_h >= 1, (n_e, n_s, n_h)
assert d["n_action_rows_ok"]    == 4, d
assert d["n_action_execute"]    == n_e, d
assert d["n_action_simplify"]   == n_s, d
assert d["n_action_ask_human"]  == n_h, d

P = d["percep"]
assert [x["name"] for x in P] == ["camera","lidar","ultrasonic"], P
n_f = n_x = 0
for x in P:
    exp = x["sigma_local"] <= d["tau_fuse"]
    assert x["fused_in"] == exp, x
    if x["fused_in"]: n_f += 1
    else:              n_x += 1
assert n_f >= 1 and n_x >= 1, (n_f, n_x)
assert d["n_percep_rows_ok"]   == 3, d
assert d["n_percep_fused"]     == n_f, d
assert d["n_percep_excluded"]  == n_x, d
assert d["sigma_percep_fused"] < d["sigma_percep_naive"], d
assert d["perception_gain_ok"] is True, d

S = d["safety"]
assert len(S) == 4
for i in range(1, 4):
    assert S[i]["sigma_safety"] > S[i-1]["sigma_safety"], S
    assert S[i]["slow_factor"]  < S[i-1]["slow_factor"],  S
assert d["n_safety_rows_ok"]  == 4, d
assert d["safety_monotone_ok"] is True, d

F = d["fail"]
assert len(F) == 3
for f in F:
    assert f["sigma_current"] > f["sigma_prior"], f
    assert f["learned"] is True, f
assert d["n_fail_rows_ok"]   == 3, d
assert d["n_fail_learned"]   == 3, d

assert 0.0 <= d["sigma_robotics"] <= 1.0, d
assert d["sigma_robotics"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v273: non-deterministic" >&2; exit 1; }
