#!/usr/bin/env bash
#
# v231 σ-Immortal — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 10 steps; every delta popcount ≤ 8
#   * incremental_bits < full_per_step_bits
#   * σ_continuity == 0 (restored == original for every t)
#   * σ_transplant == 0 and target_identity ≠ source_identity
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v231_immortal"
[ -x "$BIN" ] || { echo "v231: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v231", d
assert d["n_steps"] == 10, d
assert d["chain_valid"] is True, d
assert d["incremental_bits"] < d["full_per_step_bits"], d

for sn in d["trajectory"][1:]:
    assert sn["delta_popcount"] <= d["delta_budget"], sn
    assert sn["restored"] == sn["state"], sn

assert d["sigma_continuity"] == 0.0, d
assert d["sigma_transplant"] == 0.0, d
assert d["target_identity"]  != d["source_identity"], d
assert d["target_skills"]    == d["source_last_skills"], d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v231: non-deterministic" >&2; exit 1; }
