#!/usr/bin/env bash
#
# v272 σ-Digital-Twin — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 sync fixtures; stable == (σ_twin < 0.05);
#     drifted == (σ_twin > 0.30); >=1 stable AND
#     >=1 drifted
#   * 3 maintenance rows; action ∈ {REPLACE, MONITOR};
#     REPLACE iff σ_prediction <= 0.30; both branches
#   * 3 what-if rows; decision ∈ {IMPLEMENT, ABORT};
#     IMPLEMENT iff σ_whatif <= 0.25; both branches
#   * 3 verified-action rows; σ_match ==
#     |declared_sim − realized_phys|; verdict
#     ∈ {PASS, BLOCK}; PASS iff σ_match <= 0.10; both
#     branches
#   * sigma_digital_twin in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v272_digital_twin"
[ -x "$BIN" ] || { echo "v272: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v272", d
assert d["chain_valid"] is True, d
assert abs(d["tau_pred"]   - 0.30) < 1e-6, d
assert abs(d["tau_whatif"] - 0.25) < 1e-6, d
assert abs(d["tau_match"]  - 0.10) < 1e-6, d
assert abs(d["stable_thr"] - 0.05) < 1e-6, d
assert abs(d["drift_thr"]  - 0.30) < 1e-6, d

S = d["sync"]
assert len(S) == 4
n_s = n_dr = 0
for x in S:
    assert x["stable"]  == (x["sigma_twin"] <  d["stable_thr"]), x
    assert x["drifted"] == (x["sigma_twin"] >  d["drift_thr"]),  x
    if x["stable"]:  n_s  += 1
    if x["drifted"]: n_dr += 1
assert n_s >= 1 and n_dr >= 1, (n_s, n_dr)
assert d["n_sync_rows_ok"]  == 4,  d
assert d["n_sync_stable"]   == n_s, d
assert d["n_sync_drifted"]  == n_dr, d

P = d["pred"]
assert len(P) == 3
n_r = n_m = 0
for p in P:
    exp = "REPLACE" if p["sigma_prediction"] <= d["tau_pred"] else "MONITOR"
    assert p["action"] == exp, p
    assert p["predicted_failure_hours"] > 0, p
    if p["action"] == "REPLACE": n_r += 1
    else:                         n_m += 1
assert n_r >= 1 and n_m >= 1, (n_r, n_m)
assert d["n_pred_rows_ok"] == 3, d
assert d["n_pred_replace"] == n_r, d
assert d["n_pred_monitor"] == n_m, d

W = d["whatif"]
assert len(W) == 3
n_i = n_a = 0
for w in W:
    exp = "IMPLEMENT" if w["sigma_whatif"] <= d["tau_whatif"] else "ABORT"
    assert w["decision"] == exp, w
    if w["decision"] == "IMPLEMENT": n_i += 1
    else:                              n_a += 1
assert n_i >= 1 and n_a >= 1, (n_i, n_a)
assert d["n_whatif_rows_ok"]    == 3, d
assert d["n_whatif_implement"]  == n_i, d
assert d["n_whatif_abort"]      == n_a, d

V = d["verified"]
assert len(V) == 3
n_p = n_b = 0
for v in V:
    expected = abs(v["declared_sim"] - v["realized_phys"])
    assert abs(v["sigma_match"] - expected) < 1e-5, v
    exp = "PASS" if v["sigma_match"] <= d["tau_match"] else "BLOCK"
    assert v["verdict"] == exp, v
    if v["verdict"] == "PASS": n_p += 1
    else:                        n_b += 1
assert n_p >= 1 and n_b >= 1, (n_p, n_b)
assert d["n_verified_rows_ok"] == 3, d
assert d["n_verified_pass"]    == n_p, d
assert d["n_verified_block"]   == n_b, d

assert 0.0 <= d["sigma_digital_twin"] <= 1.0, d
assert d["sigma_digital_twin"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v272: non-deterministic" >&2; exit 1; }
