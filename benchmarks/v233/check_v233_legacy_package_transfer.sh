#!/usr/bin/env bash
#
# v233 σ-Legacy — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 10 items, sorted by σ ascending
#   * σ ∈ [0,1] AND utility ∈ [0,1] per item
#   * adopted iff σ ≤ τ_adopt
#   * n_adopted ≥ 3 AND n_discarded ≥ 1 AND sum = n_items
#   * σ_legacy = adopted_utility / total_utility ∈ (0, 1]
#   * successor_id ≠ predecessor_id, shutdown == true
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v233_legacy"
[ -x "$BIN" ] || { echo "v233: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v233", d
assert d["n_items"] == 10, d
assert d["chain_valid"] is True, d
assert d["predecessor_shutdown"] is True, d
assert d["successor_id"] != d["predecessor_id"], d

prev_s = -1.0
for i, it in enumerate(d["items"]):
    assert it["sigma_rank"] == i, it
    assert 0.0 <= it["sigma"] <= 1.0, it
    assert 0.0 <= it["utility"] <= 1.0, it
    assert it["sigma"] >= prev_s - 1e-9, (prev_s, it)
    prev_s = it["sigma"]
    assert it["adopted"] is (it["sigma"] <= d["tau_adopt"] + 1e-9), it

assert d["n_adopted"]   >= 3, d
assert d["n_discarded"] >= 1, d
assert d["n_adopted"] + d["n_discarded"] == d["n_items"], d

import math
tu = d["total_utility"]
au = d["adopted_utility"]
assert tu > 0.0, d
exp_sl = au / tu
assert abs(d["sigma_legacy"] - exp_sl) < 1e-4, (d["sigma_legacy"], exp_sl)
assert 0.0 < d["sigma_legacy"] <= 1.0 + 1e-9, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v233: non-deterministic" >&2; exit 1; }
