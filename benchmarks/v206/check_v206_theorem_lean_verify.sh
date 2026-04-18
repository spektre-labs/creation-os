#!/usr/bin/env bash
#
# v206 σ-Theorem — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * ≥ 1 PROVEN, ≥ 1 CONJECTURE, ≥ 1 SPECULATION, ≥ 1 REFUTED
#   * PROVEN ⇒ lean_accepts AND σ_proof ≤ τ_step
#   * REFUTED ⇒ counter-example id ≠ 0
#   * all σ ∈ [0,1]; σ_proof = max(σ_step)
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v206_theorem"
[ -x "$BIN" ] || { echo "v206: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v206", d
assert d["n"] == 8, d
for k in ("n_proven", "n_conjecture", "n_speculation", "n_refuted"):
    assert d[k] >= 1, (k, d)
assert d["chain_valid"] is True, d

tau = d["tau_step"]
for t in d["theorems"]:
    assert t["status"] in (0, 1, 2, 3), t
    for f in ("sf", "sp"):
        assert 0.0 <= t[f] <= 1.0 + 1e-6, (f, t)
    if t["status"] == 0:   # PROVEN
        assert t["lean"] is True, t
        assert t["sp"] <= tau + 1e-6, t
    if t["status"] == 3:   # REFUTED
        assert t["counter"] != 0, t
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v206: non-deterministic" >&2; exit 1; }
