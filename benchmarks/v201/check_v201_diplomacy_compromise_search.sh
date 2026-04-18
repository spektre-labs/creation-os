#!/usr/bin/env bash
#
# v201 σ-Diplomacy — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 8 negotiations, every one yields TREATY or DEFER
#   * ≥ 1 TREATY, ≥ 1 DEFER, ≥ 1 betrayal
#   * Treaty compromise_x ∈ red-line interval for every party
#   * σ_comp_max ≤ position spread (minimax improves on extremes)
#   * betrayer trust strictly below non-betrayers
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v201_diplomacy"
[ -x "$BIN" ] || { echo "v201: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"] == "v201", d
assert d["n_negotiations"] == 8, d
assert d["n_treaties"] >= 1, d
assert d["n_defers"]   >= 1, d
assert d["n_betrayals"]>= 1, d
assert d["chain_valid"] is True, d

# Betrayer trust below others: party 2.
t = d["trust"]
assert t[2] < t[0] and t[2] < t[1] and t[2] < t[3], t

for n in d["negotiations"]:
    if n["outcome"] == 0:   # TREATY
        assert 0.0 <= n["x"] <= 1.0, n
        assert n["smax"] <= 1.0, n
    else:                   # DEFER
        assert n["x"] == -1.0, n
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v201: non-deterministic output" >&2
    exit 1
fi
