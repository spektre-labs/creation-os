#!/usr/bin/env bash
#
# v205 σ-Experiment — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 experiments (one per v204 top-3)
#   * each has distinct dep/indep/control variable ids
#   * n_required deterministic from (α, β, effect) — matches
#     (z_α + z_β)^2 / effect^2
#   * ≥ 1 SIM_SUPPORTS and ≥ 1 UNDER_POWERED decision
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v205_experiment"
[ -x "$BIN" ] || { echo "v205: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json, math
d = json.loads("""$OUT""")
assert d["kernel"] == "v205", d
assert d["n"] == 3, d
assert d["n_sim_supports"]  >= 1, d
assert d["n_under_powered"] >= 1, d
assert d["chain_valid"]     is True, d

z_a = 1.96
z_b = 0.8416
for e in d["experiments"]:
    roles = {e["dep"], e["indep"], e["ctrl"]}
    assert len(roles) == 3, e
    expected = math.ceil(((z_a + z_b) ** 2) / (e["eff"] ** 2))
    assert e["n_required"] == expected, (e, expected)
    for f in ("sd", "ss", "sp"):
        assert 0.0 <= e[f] <= 1.0 + 1e-6, (f, e)
    assert e["decision"] in (0, 1, 2), e
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v205: non-deterministic" >&2; exit 1; }
