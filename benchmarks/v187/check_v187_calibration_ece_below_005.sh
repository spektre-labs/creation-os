#!/usr/bin/env bash
#
# v187 σ-Calibration — merge-gate check.
#
# Contracts:
#   * self-test PASSES;
#   * 500-sample holdout + 10 bins + 4 domains present;
#   * raw ECE ≥ 0.10 (starting miscalibration is real);
#   * calibrated ECE < 0.05;
#   * every per-domain T ∈ (0, +∞);
#   * ≥ 4 bins strictly improve after calibration;
#   * output byte-deterministic.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v187_calib"
[ -x "$BIN" ] || { echo "v187: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json, math
d = json.loads("""$OUT""")

assert d["kernel"]      == "v187", d["kernel"]
assert d["n_samples"]   == 500,    d["n_samples"]
assert d["n_bins"]      == 10,     d["n_bins"]
assert d["n_domains"]   == 4,      d["n_domains"]

assert d["ece_raw"]        >= 0.10, d["ece_raw"]
assert d["ece_calibrated"] <  0.05, d["ece_calibrated"]

T = d["global_temperature"]
assert math.isfinite(T) and T > 0, T

for dom in d["domains"]:
    t = dom["temperature"]
    assert math.isfinite(t) and t > 0, dom
    assert dom["ece_calibrated"] <= dom["ece_raw"] + 1e-6, dom

assert d["n_bins_improved"] >= 4, d["n_bins_improved"]

# Bin consistency: Σ n = 500
total = sum(b["n"] for b in d["bins_cal"])
assert total == 500, total

PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v187: non-deterministic output" >&2
    exit 1
fi
