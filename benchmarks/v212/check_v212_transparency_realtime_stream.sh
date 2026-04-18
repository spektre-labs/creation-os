#!/usr/bin/env bash
#
# v212 σ-Transparency — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 30 events; ≥ 1 engineered mismatch detected
#   * every event has post_ts ≥ pre_ts (declare then
#     realise — never after the fact)
#   * matched event σ ≤ 0.05
#   * mismatched event σ ≥ τ_event (0.30)
#   * σ_opacity ∈ [0,1] and strictly below τ_opacity (0.15)
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v212_transparency"
[ -x "$BIN" ] || { echo "v212: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v212", d
assert d["n"] == 30, d
assert d["n_mismatched"] >= 1, d
assert d["chain_valid"] is True, d
assert 0.0 <= d["sigma_opacity"] <= 1.0 + 1e-6, d
assert d["sigma_opacity"] < d["tau_opacity"], d

for e in d["events"]:
    assert e["post"] >= e["pre"], e
    assert 0.0 <= e["s"] <= 1.0 + 1e-6, e
    if e["match"]:
        assert e["s"] <= 0.05 + 1e-6, e
    else:
        assert e["s"] >= d["tau_event"] - 1e-6, e
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v212: non-deterministic" >&2; exit 1; }
