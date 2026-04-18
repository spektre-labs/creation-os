#!/usr/bin/env bash
#
# v179 σ-Interpret — merge-gate check.
#
# Contracts:
#   * self-test exits 0 on the synthetic fixture;
#   * SAE produces ≥ 5 features with |Pearson r| ≥ τ_correlated;
#   * top-correlated feature has a non-empty, non-"unlabeled" label
#     and |r| ≥ 0.60;
#   * `--explain N` returns a non-empty mechanistic explanation
#     that mentions the σ value, the SAE feature id, and the
#     n_effective collapse (before → after);
#   * output is fully deterministic on a fixed seed.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v179_interpret"

if [ ! -x "$BIN" ]; then
    echo "v179: $BIN not built" >&2
    exit 1
fi

# 1. Self-test ----------------------------------------------------
"$BIN" --self-test >/dev/null

# 2. Capture JSON features ---------------------------------------
OUT="$("$BIN")"

N_CORR=$(echo "$OUT" | python3 -c 'import sys,json; print(json.loads(sys.stdin.read())["n_features_correlated"])')
if [ "$N_CORR" -lt 5 ]; then
    echo "v179: expected ≥5 correlated features, got $N_CORR" >&2
    exit 1
fi

TOP_LABEL=$(echo "$OUT" | python3 -c 'import sys,json; print(json.loads(sys.stdin.read())["features"][0]["label"])')
TOP_R=$(echo "$OUT" | python3 -c 'import sys,json; print(json.loads(sys.stdin.read())["features"][0]["r"])')

if [ -z "$TOP_LABEL" ] || [ "$TOP_LABEL" = "unlabeled-feature" ]; then
    echo "v179: top feature has no label ($TOP_LABEL)" >&2
    exit 1
fi

python3 -c "
import sys
r = float('$TOP_R')
if abs(r) < 0.60:
    print('v179: top |r|=%.3f < 0.60' % abs(r), file=sys.stderr)
    sys.exit(1)
"

# 3. Per-sample explanation --------------------------------------
# Find a high-σ sample — sample 0 in fixture may be low; scan
# the first 16 samples and take the first one whose σ ≥ 0.45.
FOUND=""
for i in $(seq 0 15); do
    EX="$("$BIN" --explain "$i")"
    S=$(echo "$EX" | python3 -c 'import sys,json; print(json.loads(sys.stdin.read())["sigma_product"])')
    HI=$(python3 -c "print(1 if float('$S') >= 0.45 else 0)")
    if [ "$HI" = "1" ]; then FOUND="$EX"; break; fi
done

if [ -z "$FOUND" ]; then
    echo "v179: no high-σ sample in first 16" >&2
    exit 1
fi

python3 - <<PY
import json
ex = json.loads("""$FOUND""")
expl = ex["explanation"]
fid  = ex["feature_id"]
assert expl, "empty explanation"
assert f"#{fid}" in expl,        f"explanation missing feature id: {expl}"
assert "sigma=" in expl,         f"explanation missing sigma: {expl}"
assert ex["n_eff_after"] < ex["n_eff_before"], \
    f"n_effective did not collapse: {ex['n_eff_before']} -> {ex['n_eff_after']}"
PY

# 4. Determinism --------------------------------------------------
A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v179: non-deterministic output" >&2
    exit 1
fi
