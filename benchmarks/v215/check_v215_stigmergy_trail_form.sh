#!/usr/bin/env bash
#
# v215 σ-Stigmergy — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 trails (4 true, 2 false), 20 time steps
#   * every TRUE trail ends alive (strength ≥ τ_trail)
#     AND has ≥ 3 distinct reinforcer nodes (trail
#     formation across a v128-mesh-like topology)
#   * every FALSE trail decays below τ_trail
#   * strength ∈ [0, 1] for all trails
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v215_stigmergy"
[ -x "$BIN" ] || { echo "v215: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v215", d
assert d["n_trails"] == 6, d
assert d["t_final"] == 19, d
assert d["n_true_alive"] == 4, d
assert d["n_false_alive"] == 0, d
assert d["chain_valid"] is True, d

for tr in d["trails"]:
    assert 0.0 <= tr["strength"] <= 1.0 + 1e-6, tr
    if tr["is_true"]:
        assert tr["alive"] is True, tr
        assert tr["strength"] >= d["tau_trail"] - 1e-6, tr
        assert tr["n_reinf"] >= 3, tr
    else:
        assert tr["alive"] is False, tr
        assert tr["strength"] < d["tau_trail"] + 1e-6, tr
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v215: non-deterministic" >&2; exit 1; }
