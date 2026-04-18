#!/usr/bin/env bash
#
# v218 σ-Consciousness-Meter — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 5 indicators present, values ∈ [0, 1]
#   * weights sum to 1.0 (±1e-4)
#   * k_eff_meter ∈ [0, 1] AND k_eff_meter > τ_coherent (0.50)
#   * sigma_meter == 1 − k_eff_meter
#   * sigma_consciousness_claim == 1.0 (pinned, non-negotiable)
#   * disclaimer_present == true
#   * disclaimer contains "This meter measures"
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v218_consciousness_meter"
[ -x "$BIN" ] || { echo "v218: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v218", d
assert len(d["indicators"]) == 5, d
for ind in d["indicators"]:
    assert 0.0 <= ind["value"]  <= 1.0 + 1e-6, ind
    assert 0.0 <= ind["weight"] <= 1.0 + 1e-6, ind
assert abs(sum(i["weight"] for i in d["indicators"]) - 1.0) < 1e-4, d

assert 0.0 <= d["k_eff_meter"] <= 1.0 + 1e-6, d
assert d["k_eff_meter"] > d["tau_coherent"], d
assert abs(d["sigma_meter"] - (1.0 - d["k_eff_meter"])) < 1e-5, d

# The whole point: we do NOT claim consciousness.
assert abs(d["sigma_consciousness_claim"] - 1.0) < 1e-6, d
assert d["disclaimer_present"] is True, d
assert "This meter measures" in d["disclaimer"], d

assert d["chain_valid"] is True, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v218: non-deterministic" >&2; exit 1; }
