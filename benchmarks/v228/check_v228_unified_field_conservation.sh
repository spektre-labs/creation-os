#!/usr/bin/env bash
#
# v228 σ-Unified — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * N_STEPS = 100
#   * σ(t), K_eff(t) ∈ [0, 1]
#   * |K_eff(t) · τ(t) − C| ≤ ε_cons (1e-6) for every t
#   * n_transitions ≥ 1   (trajectory crosses K_crit)
#   * σ_end < σ_start     (learning pressure wins)
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v228_unified"
[ -x "$BIN" ] || { echo "v228: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v228", d
assert d["n_steps"] == 100, d
assert d["chain_valid"] is True, d
assert len(d["trace"]) == 101, d

C = d["C"]
for sm in d["trace"]:
    assert 0.0 <= sm["sigma"] <= 1.0 + 1e-6, sm
    assert 0.0 <= sm["k_eff"] <= 1.0 + 1e-6, sm
    assert abs(sm["k_eff_tau"] - C) <= d["eps_cons"] + 1e-7, (sm, C)

assert d["n_transitions"] >= 1, d
assert d["sigma_end"] < d["sigma_start"], d
assert d["max_cons_error"] <= d["eps_cons"] + 1e-7, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v228: non-deterministic" >&2; exit 1; }
