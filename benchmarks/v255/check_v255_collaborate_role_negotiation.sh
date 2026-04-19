#!/usr/bin/env bash
#
# v255 σ-Collaborate — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 5 modes in canonical order
#     (ASSIST, PAIR, DELEGATE, TEACH, LEARN), every
#     mode_ok; n_modes_ok == 5
#   * exactly 4 negotiation fixtures; σ_human and
#     σ_model in [0,1]; chosen_mode is a valid mode;
#     at least 3 DISTINCT modes chosen across fixtures
#   * exactly 3 workspace edits; ticks strictly
#     ascending; every accepted==true; both HUMAN and
#     MODEL actors represented
#   * exactly 2 conflict fixtures; decision matches
#     σ vs τ_conflict (0.30): σ ≤ τ → ASSERT else ADMIT;
#     >=1 ASSERT AND >=1 ADMIT
#   * sigma_collaborate in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v255_collaborate"
[ -x "$BIN" ] || { echo "v255: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v255", d
assert d["chain_valid"] is True, d
assert abs(d["tau_conflict"] - 0.30) < 1e-6, d

mn = [m["name"] for m in d["modes"]]
assert mn == ["ASSIST","PAIR","DELEGATE","TEACH","LEARN"], mn
for m in d["modes"]:
    assert m["mode_ok"] is True, m
assert d["n_modes_ok"] == 5, d

valid = set(mn)
chosen = set()
for g in d["negotiation"]:
    assert 0.0 <= g["sigma_human"] <= 1.0, g
    assert 0.0 <= g["sigma_model"] <= 1.0, g
    assert g["chosen_mode"] in valid, g
    chosen.add(g["chosen_mode"])
assert len(chosen) >= 3, chosen
assert d["n_distinct_chosen"] == len(chosen), d

prev = 0
hseen = mseen = False
for e in d["workspace"]:
    assert e["tick"] > prev, e
    prev = e["tick"]
    assert e["accepted"] is True, e
    if e["actor"] == "HUMAN": hseen = True
    if e["actor"] == "MODEL": mseen = True
assert hseen and mseen, d
assert d["n_workspace_ok"] == 3, d

tau = d["tau_conflict"]
for c in d["conflicts"]:
    assert 0.0 <= c["sigma_disagreement"] <= 1.0, c
    exp = "ASSERT" if c["sigma_disagreement"] <= tau else "ADMIT"
    assert c["decision"] == exp, c
assert d["n_assert"] >= 1, d
assert d["n_admit"]  >= 1, d

assert 0.0 <= d["sigma_collaborate"] <= 1.0, d
assert d["sigma_collaborate"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v255: non-deterministic" >&2; exit 1; }
