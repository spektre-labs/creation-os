#!/usr/bin/env bash
#
# v221 σ-Language — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 10 utterances × 4 languages (en/fi/ja/es)
#   * every σ ∈ [0, 1]
#   * every implicature caught (σ_implicature ≤ 0.10)
#   * discourse coherent ≥ 7/10
#   * multilingual calibration: |μ_L − μ_global| ≤ Δ_calib
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v221_language"
[ -x "$BIN" ] || { echo "v221: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v221", d
assert d["n_utterances"] == 10 and d["n_langs"] == 4, d
assert d["chain_valid"] is True, d
assert d["lang_calibrated"] is True, d

n_imp = sum(1 for u in d["utterances"] if u["imp"])
assert d["n_implicatures_caught"] == n_imp, d
assert d["n_discourse_coherent"] >= 7, d

for u in d["utterances"]:
    for k in ("ss", "si", "sd", "sl"):
        assert 0.0 <= u[k] <= 1.0 + 1e-6, u
    assert u["si"] <= 0.10 + 1e-6, u

mu = d["global_mean"]
for m in d["per_lang_mean"]:
    assert abs(m - mu) <= d["delta_calib"] + 1e-6, (m, mu, d)

w = d["weights"]
assert abs(sum(w) - 1.0) < 1e-6, w
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v221: non-deterministic" >&2; exit 1; }
