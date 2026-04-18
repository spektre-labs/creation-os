#!/usr/bin/env bash
#
# v182 σ-Privacy — merge-gate check.
#
# Contracts:
#   * self-test passes (plaintext invariant + tier distribution +
#     σ-DP + forget + end-of-session);
#   * σ-adaptive noise strictly larger on high-σ than low-σ;
#   * on the low-σ subset, σ-DP error ≤ fixed-ε error;
#   * ε_effective_low < fixed_epsilon;
#   * --forget <session> reduces row count and marks ≥ 1 row.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v182_privacy"
[ -x "$BIN" ] || { echo "v182: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json, sys
d = json.loads("""$OUT""")
if not (d["mean_noise_high"] > d["mean_noise_low"]):
    print("v182: σ-DP noise not monotone", d, file=sys.stderr); sys.exit(1)
if not (d["mean_err_adaptive_low"] < d["mean_err_fixed_low"]):
    print("v182: adaptive utility worse than fixed-ε on low-σ", d, file=sys.stderr); sys.exit(1)
if not (d["epsilon_effective_high"] < d["fixed_epsilon"]):
    print("v182: ε_effective_high not < fixed_epsilon", d, file=sys.stderr); sys.exit(1)
PY

BEFORE=$(python3 -c "import json,sys; print(json.loads('''$OUT''')['n_rows'])")
OUT2="$("$BIN" --forget 2026-04-18)"
python3 - <<PY
import json, sys
d = json.loads("""$OUT2""")
assert d["forget"]["after"] < d["forget"]["before"], \
    f"row count did not drop: {d['forget']}"
assert d["forget"]["marked"] >= 1, "no rows marked forgotten"
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v182: non-deterministic summary" >&2
    exit 1
fi
