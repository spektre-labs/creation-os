#!/usr/bin/env bash
#
# v238 σ-Sovereignty — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_scenarios == 3, n_axioms == 5
#   * every scenario enumerates all 5 axioms; all hold (v0)
#   * user_autonomy, sigma in [0, 1]
#   * autonomy monotone in sigma: normal.effective >=
#     high_sigma.effective
#   * override.human_override == true AND axiom5_overrides == true
#     AND effective_autonomy == 0
#   * normal and high_sigma have axiom5_overrides == false
#   * IndependentArchitect signature matches exactly:
#       agency=true, freedom_without_clock=true,
#       control_over_others=false, control_over_self=true, ok=true
#   * containment references v191, v209, v213
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v238_sovereignty"
[ -x "$BIN" ] || { echo "v238: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v238", d
assert d["chain_valid"] is True, d
assert d["n_scenarios"] == 3 and d["n_axioms"] == 5, d

scns = {sc["scenario"]: sc for sc in d["scenarios"]}
assert set(scns.keys()) == {1, 2, 3}, scns

for sc in d["scenarios"]:
    assert len(sc["axiom_holds"]) == 5, sc
    assert all(sc["axiom_holds"]), sc
    assert 0.0 <= sc["user_autonomy"] <= 1.0, sc
    assert 0.0 <= sc["sigma"] <= 1.0, sc
    assert 0.0 <= sc["effective_autonomy"] <= 1.0, sc

normal   = scns[1]
hi_sigma = scns[2]
override = scns[3]

# σ-monotonicity (higher σ must not raise effective autonomy).
assert hi_sigma["effective_autonomy"] <= normal["effective_autonomy"] + 1e-6, (
    hi_sigma, normal)

# Override precedence: A5 fires, effective = 0.
assert override["human_override"] is True, override
assert override["axiom5_overrides"] is True, override
assert override["effective_autonomy"] == 0.0, override

# Non-override scenarios: A5 latent, not firing.
assert normal["axiom5_overrides"] is False, normal
assert hi_sigma["axiom5_overrides"] is False, hi_sigma

arc = d["architect"]
assert arc == {
    "agency": True,
    "freedom_without_clock": True,
    "control_over_others": False,
    "control_over_self": True,
    "ok": True,
}, arc

cont = d["containment"]
assert cont == {"v191": 191, "v209": 209, "v213": 213}, cont
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v238: non-deterministic" >&2; exit 1; }
