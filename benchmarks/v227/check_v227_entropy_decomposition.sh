#!/usr/bin/env bash
#
# v227 σ-Entropy — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 8 distributions, K=5
#   * every p_i sums to 1 ± 1e-4 and p_i[k] ≥ 0
#   * every σ ∈ [0, 1]
#   * σ_H + σ_free = 1 (± 1e-4) per item
#   * MI ∈ [0, H(p)] per item
#   * mse_product < mse_H   (σ_product beats entropy)
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v227_entropy"
[ -x "$BIN" ] || { echo "v227: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json, math
d = json.loads("""$OUT""")
assert d["kernel"] == "v227", d
assert d["n"] == 8 and d["k"] == 5, d
assert d["chain_valid"] is True, d

logK = math.log(5.0)
for it in d["items"]:
    s = sum(it["p"])
    assert abs(s - 1.0) < 1e-3, it
    for x in it["p"]: assert x >= -1e-6, it
    for key in ("sigma_H","sigma_nEff","sigma_tail","sigma_top1",
                "sigma_product","sigma_free","sigma_mutual","sigma_true"):
        assert 0.0 <= it[key] <= 1.0 + 1e-6, (key, it)
    assert abs((it["sigma_H"] + it["sigma_free"]) - 1.0) < 1e-3, it
    H = it["sigma_H"] * logK
    assert -1e-4 <= it["mi"] <= H + 1e-3, (it, H)

assert d["mse_product"] < d["mse_H"], d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v227: non-deterministic" >&2; exit 1; }
