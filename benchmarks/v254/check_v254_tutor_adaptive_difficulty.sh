#!/usr/bin/env bash
#
# v254 σ-Tutor — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 4 skills in canonical order with
#     p_mastery, sigma_mastery in [0,1], skill_ok
#   * exactly 4 exercises, sigma_difficulty =
#     |difficulty - learner_level| and <= tau_fit=0.20,
#     fit==true, n_exercises_fit==4
#   * exactly 4 modalities in canonical order, exactly
#     one chosen AND it is the one with minimum sigma_fit
#   * exactly 3 progress rows; every pct_after >=
#     pct_before AND at least one delta_pct > 0
#   * exactly 4 privacy flags, all enabled
#   * sigma_tutor in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v254_tutor"
[ -x "$BIN" ] || { echo "v254: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v254", d
assert d["chain_valid"] is True, d
assert abs(d["tau_fit"] - 0.20) < 1e-6, d

sn = [k["name"] for k in d["skills"]]
assert sn == ["linear_algebra","calculus","probability","discrete_math"], sn
for k in d["skills"]:
    assert 0.0 <= k["p_mastery"]     <= 1.0, k
    assert 0.0 <= k["sigma_mastery"] <= 1.0, k
    assert k["skill_ok"] is True, k
assert d["n_skills_ok"] == 4, d

tf = d["tau_fit"]
for e in d["exercises"]:
    assert abs(e["sigma_difficulty"] -
               abs(e["difficulty"] - e["learner_level"])) < 1e-6, e
    assert e["sigma_difficulty"] <= tf, e
    assert e["fit"] is True, e
assert d["n_exercises_fit"] == 4, d

mn = [m["name"] for m in d["modalities"]]
assert mn == ["text","code","simulation","dialog"], mn
min_sf = min(m["sigma_fit"] for m in d["modalities"])
chosen = [m for m in d["modalities"] if m["chosen"]]
assert len(chosen) == 1, chosen
assert abs(chosen[0]["sigma_fit"] - min_sf) < 1e-6, chosen
assert d["n_chosen_modalities"] == 1, d

assert len(d["progress"]) == 3, d
assert all(p["pct_after"] >= p["pct_before"] for p in d["progress"]), d
assert any(p["delta_pct"]  > 0               for p in d["progress"]), d

pn = [p["flag"] for p in d["privacy"]]
assert pn == ["local_only","no_third_party","user_owned_data","export_supported"], pn
for p in d["privacy"]:
    assert p["enabled"] is True, p
assert d["n_privacy_enabled"] == 4, d

assert 0.0 <= d["sigma_tutor"] <= 1.0, d
assert d["sigma_tutor"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v254: non-deterministic" >&2; exit 1; }
