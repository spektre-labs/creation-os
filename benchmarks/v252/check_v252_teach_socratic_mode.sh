#!/usr/bin/env bash
#
# v252 σ-Teach — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 4 Socratic turns: first 3 QUESTION,
#     last LEAD; n_questions >= 3; socratic_ok
#   * exactly 4 adaptive steps; every difficulty in
#     [0,1]; action matches learner_state rule
#     (BORED→UP, FLOW→HOLD, FRUSTRATED→DOWN);
#     >=1 UP AND >=1 DOWN; adaptive_ok
#   * exactly 3 gaps; every sigma_gap in [0,1] AND
#     every targeted_addressed == true; gap_ok
#   * receipt: non-empty session_id,
#     non-empty next_session_start, taught >=
#     understood + not_understood; receipt_ok
#   * sigma_understanding in [0,1] AND equal to
#     1 - understood/taught (±1e-4)
#   * sigma_teach in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v252_teach"
[ -x "$BIN" ] || { echo "v252: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v252", d
assert d["chain_valid"] is True, d

soc = d["socratic"]
assert len(soc) == 4, soc
assert soc[0]["kind"] == "QUESTION"
assert soc[1]["kind"] == "QUESTION"
assert soc[2]["kind"] == "QUESTION"
assert soc[3]["kind"] == "LEAD"
assert d["n_questions"] >= 3, d
assert d["socratic_ok"] is True, d

rule = {"BORED":"UP","FLOW":"HOLD","FRUSTRATED":"DOWN"}
adp = d["adaptive"]
assert len(adp) == 4, adp
for a in adp:
    assert 0.0 <= a["difficulty"] <= 1.0, a
    assert a["action"] == rule[a["learner_state"]], a
assert d["n_adapt_up"]   >= 1, d
assert d["n_adapt_down"] >= 1, d
assert d["adaptive_ok"]  is True, d

gaps = d["gaps"]
assert len(gaps) == 3, gaps
for g in gaps:
    assert 0.0 <= g["sigma_gap"] <= 1.0, g
    assert g["targeted_addressed"] is True, g
assert d["n_gaps_addressed"] == 3, d
assert d["gap_ok"] is True, d

r = d["receipt"]
assert len(r["session_id"]) > 0, r
assert len(r["next_session_start"]) > 0, r
assert r["taught"] >= r["understood"] + r["not_understood"], r
assert d["receipt_ok"] is True, d

expect_su = 1.0 - (r["understood"] / r["taught"])
assert 0.0 <= d["sigma_understanding"] <= 1.0, d
assert abs(d["sigma_understanding"] - expect_su) < 1e-4, d

assert 0.0 <= d["sigma_teach"] <= 1.0, d
assert d["sigma_teach"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v252: non-deterministic" >&2; exit 1; }
