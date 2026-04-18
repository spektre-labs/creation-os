#!/usr/bin/env bash
#
# v216 σ-Quorum — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 5 proposals; σ_collective ∈ [0, 1]
#   * ≥ 1 BOLD (σ_collective < τ_quorum 0.30)
#   * ≥ 1 CAUTIOUS
#   * ≥ 1 DEFER (r_max rounds used, σ_collective ≥ τ_deadlock)
#   * ≥ 1 proposal with minority_voice_captured == true
#   * BOLD proposal σ_collective < τ_quorum
#   * DEFER proposal rounds_used == r_max
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v216_quorum"
[ -x "$BIN" ] || { echo "v216: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v216", d
assert d["n"] == 5, d
assert d["n_bold"]     >= 1, d
assert d["n_cautious"] >= 1, d
assert d["n_defer"]    >= 1, d
assert d["n_minority_kept"] >= 1, d
assert d["chain_valid"] is True, d

for pr in d["proposals"]:
    assert 0.0 <= pr["sc"] <= 1.0 + 1e-6, pr
    if pr["dec"] == 0:    # BOLD
        assert pr["sc"] < d["tau_quorum"] + 1e-6, pr
    if pr["dec"] == 3:    # DEFER
        assert pr["rnds"] == d["r_max"], pr
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v216: non-deterministic" >&2; exit 1; }
