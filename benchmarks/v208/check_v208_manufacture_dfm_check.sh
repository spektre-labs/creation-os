#!/usr/bin/env bash
#
# v208 σ-Manufacture — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 manufacturing runs
#   * ≥ 1 DFM issue flagged across the fleet
#   * σ_process_max, σ_quality, σ_dfm ∈ [0,1]
#   * higher σ_quality ⇒ ≥ checkpoints (v159 heal)
#   * every run carries a non-zero feedback hypothesis id
#     (closed loop v208 → v204)
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v208_manufacture"
[ -x "$BIN" ] || { echo "v208: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v208", d
assert d["n"] == 4, d
assert d["total_dfm_flagged"] >= 1, d
assert d["chain_valid"] is True, d

runs = d["runs"]
for r in runs:
    assert 0.0 <= r["pmax"] <= 1.0 + 1e-6, r
    assert 0.0 <= r["q"]    <= 1.0 + 1e-6, r
    for f in r["dfm"]:
        assert 0.0 <= f["s"] <= 1.0 + 1e-6, f
        if f["flag"]:
            assert f["sug"] != 0, f
    assert r["fb"] != 0, r   # closed loop
    assert r["cp"] >= 2, r

# Monotone: higher q ⇒ ≥ checkpoints.
for a in runs:
    for b in runs:
        if a["q"] >= b["q"] - 1e-6:
            assert a["cp"] >= b["cp"], (a, b)
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v208: non-deterministic" >&2; exit 1; }
