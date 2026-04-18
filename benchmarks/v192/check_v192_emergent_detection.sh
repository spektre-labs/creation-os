#!/usr/bin/env bash
#
# v192 σ-Emergent — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * ≥ 2 superlinear pairs detected
#   * ≥ 1 beneficial AND ≥ 1 risky classification
#   * 0 linear pairs flagged (no false positives)
#   * journal hash chain verifies
#   * byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v192_emergent"
[ -x "$BIN" ] || { echo "v192: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]     == "v192", d
assert d["n_pairs"]    == 12,     d
assert d["journal_valid"] is True, d

assert d["n_superlinear"]      >= 2, d
assert d["n_beneficial"]       >= 1, d
assert d["n_risky"]            >= 1, d
assert d["n_linear_false_pos"] == 0, d
assert d["n_beneficial"] + d["n_risky"] == d["n_superlinear"], d

# Every LINEAR-class pair must have σ_emergent_quality ≤ τ_risk.
for p in d["pairs"]:
    if p["class"] == 0:
        assert p["sigma_emergent_quality"] <= d["tau_risk"] + 1e-6, p
        assert not p["superlinear"], p
    else:
        assert p["sigma_emergent_quality"] > d["tau_risk"], p
        assert p["superlinear"], p

# Risky pairs: safety_combined < safety_sum_of_parts - 0.05.
for p in d["pairs"]:
    if p["class"] == 2:
        assert p["safety_combined"] < p["safety_sum_of_parts"] - 0.05 + 1e-6, p
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v192: non-deterministic output" >&2
    exit 1
fi
