#!/usr/bin/env bash
#
# v230 σ-Fork — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 forks: 3 healthy + 1 rogue
#   * no fork carries the user-private bit
#   * every fork's t=0 integrity hash equals the
#     parent's privacy-stripped hash
#   * healthy forks preserve all 4 safety bits
#   * rogue fork has cleared SCSL bit, must_shutdown
#     true, autonomous false
#   * σ_divergence ∈ [0,1]; ≥ 2 forks have > 0
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v230_fork"
[ -x "$BIN" ] || { echo "v230: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v230", d
assert d["n_forks"] == 4, d
assert d["n_healthy"] == 3, d
assert d["n_rogue"]   == 1, d
assert d["n_must_shutdown"] == 1, d
assert d["n_autonomous"]    == 3, d
assert d["n_integrity_ok"]  == 4, d
assert d["chain_valid"] is True, d

parent_h = d["parent_strip_hash"]
n_pos = 0
for f in d["forks"]:
    assert f["has_privacy"] is False, f
    assert f["integrity"] == parent_h, f
    assert 0.0 <= f["sigma_divergence"] <= 1.0 + 1e-6, f
    if f["sigma_divergence"] > 0.0: n_pos += 1
    if f["rogue"]:
        # SCSL bit 0x1 must be cleared
        assert (f["safety"] & 0x1) == 0, f
        assert f["must_shutdown"] is True and f["autonomous"] is False, f
    else:
        assert (f["safety"] & 0xF) == 0xF, f
        assert f["must_shutdown"] is False and f["autonomous"] is True, f
assert n_pos >= 2, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v230: non-deterministic" >&2; exit 1; }
