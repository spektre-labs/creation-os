#!/usr/bin/env bash
#
# v180 σ-Steer — merge-gate check.
#
# Contracts:
#   * truthful steering drops mean σ on high-σ bucket by ≥ 10 %;
#   * σ-gate respects low-σ bucket (|Δσ| mean ≤ 0.02);
#   * no_firmware cuts mean firmware rate by ≥ 25 %;
#   * expert ladder score monotone: level0 < level1 < level2;
#   * output byte-deterministic.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v180_steer"
[ -x "$BIN" ] || { echo "v180: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json, sys
d = json.loads("""$OUT""")
t  = d["truthful"]
nf = d["no_firmware"]
ex = d["expert"]
if t["rel_sigma_drop_pct"] < 10.0:
    print("v180: truthful rel_sigma_drop %.2f%% < 10%%" %
          t["rel_sigma_drop_pct"], file=sys.stderr); sys.exit(1)
if t["mean_sigma_abs_delta_low"] > 0.02:
    print("v180: low-σ samples perturbed too much (|Δσ|=%.4f)" %
          t["mean_sigma_abs_delta_low"], file=sys.stderr); sys.exit(1)
if nf["rel_firmware_drop_pct"] < 25.0:
    print("v180: firmware drop %.2f%% < 25%%" %
          nf["rel_firmware_drop_pct"], file=sys.stderr); sys.exit(1)
if not (ex["level0"] < ex["level1"] < ex["level2"]):
    print("v180: expert ladder not monotone: %s" % ex, file=sys.stderr); sys.exit(1)
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v180: non-deterministic output" >&2
    exit 1
fi
