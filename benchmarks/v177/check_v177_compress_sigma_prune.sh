#!/usr/bin/env bash
#
# benchmarks/v177/check_v177_compress_sigma_prune.sh
#
# Merge-gate for v177 σ-Compress.  Verifies:
#   1. self-test
#   2. report: 16→N layers, pruning happened, params dropped
#      materially (≥ 30 %)
#   3. precision mix: ≥1 layer at INT8, ≥1 at INT4 AND ≥1 at INT2
#   4. ≥ 1 layer merged, n_layers_after + merged == 16
#   5. σ_drift_pct ≤ drift_budget_pct (5.0 %)
#   6. bytes_after < bytes_before
#   7. determinism
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v177_compress"
test -x "$BIN" || { echo "v177: binary not built"; exit 1; }

echo "[v177] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v177] (2..6) full JSON"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v177_compress"]).decode())
r = doc["report"]
assert r["n_layers_before"] == 16, r
assert r["n_neurons_pruned"] > 0, r
assert r["params_after"] < r["params_before"], r
ratio = r["params_after"] / r["params_before"]
assert ratio < 0.70, ratio
assert r["n_layers_int8"] >= 1, r
assert r["n_layers_int4"] >= 1, r
assert r["n_layers_int2"] >= 1, r
assert r["n_layers_merged"] >= 1, r
assert r["n_layers_after"] + r["n_layers_merged"] == r["n_layers_before"], r
assert r["sigma_drift_pct"] <= doc["drift_budget_pct"] + 1e-6, r
assert r["bytes_after"] < r["bytes_before"], r
print("report ok: params %d→%d (%.1f%%), drift %.3f%%, prec %d/%d/%d, merged %d" % (
    r["params_before"], r["params_after"],
    100 * (1 - ratio), r["sigma_drift_pct"],
    r["n_layers_int8"], r["n_layers_int4"], r["n_layers_int2"],
    r["n_layers_merged"]))
PY

echo "[v177] (7) determinism"
A="$("$BIN")";       B="$("$BIN")";       [ "$A" = "$B" ] || { echo "json nondet"; exit 7; }

echo "[v177] ALL CHECKS PASSED"
