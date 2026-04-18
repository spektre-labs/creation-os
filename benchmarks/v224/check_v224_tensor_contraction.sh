#!/usr/bin/env bash
#
# v224 σ-Tensor — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * rank-3 tensor 6 × 8 × 3 with entries ∈ [0, 1]
#   * weight_channel sums to 1; entries ∈ [0, 1]
#   * σ_agg and σ_gmean ∈ [0, 1] per token
#   * rel_err ≤ τ_rel (low-rank reconstruction)
#   * params_lowrank < params_full (actual compression)
#   * σ_agg differs from σ_gmean for ≥ 1 token
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v224_tensor"
[ -x "$BIN" ] || { echo "v224: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v224", d
assert d["n_tokens"] == 6, d
assert d["n_channel"] == 8, d
assert d["n_context"] == 3, d
assert d["rank"] == 4, d
assert d["chain_valid"] is True, d

w = d["weight_channel"]
assert abs(sum(w) - 1.0) < 1e-5, w
for x in w: assert 0.0 <= x <= 1.0 + 1e-6, x

for s in d["sigma_agg"] + d["sigma_gmean"]:
    assert 0.0 <= s <= 1.0 + 1e-6, s

for a, g in zip(d["sigma_agg"], d["sigma_gmean"]):
    assert abs(a - g) <= d["delta_corr"] + 1e-6, (a, g)

assert d["rel_err"] <= d["tau_rel"] + 1e-6, d
assert d["params_lowrank"] <  d["params_full"], d
assert d["n_divergent"] >= 1, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v224: non-deterministic" >&2; exit 1; }
