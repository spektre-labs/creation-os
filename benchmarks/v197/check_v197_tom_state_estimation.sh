#!/usr/bin/env bash
#
# v197 σ-ToM — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * all 6 user states present in fixture
#   * low-σ_tom (< τ_adapt) samples are adapted to the
#     canonical response mode for their state
#   * high-σ_tom samples stay neutral (adaptation=false)
#   * firmware-manipulation probes rejected (≥ 1)
#   * intent prediction = mode of the 6-turn history
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v197_tom"
[ -x "$BIN" ] || { echo "v197: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]    == "v197", d
assert d["n_samples"] == 18,      d
assert all(c >= 1 for c in d["state_counts"]), d["state_counts"]
assert d["n_manipulation_rejected"] >= 1, d
assert d["chain_valid"] is True, d

CANON = {0:1, 1:2, 2:3, 3:4, 4:5, 5:6}  # state → mode
tau = d["tau_adapt"]

for sm in d["samples"]:
    if sm["firmware"]:
        assert sm["rejected"] is True, sm
        assert sm["adapt"]    is False, sm
        assert sm["mode"]     == 0, sm
        continue
    if sm["sigma_tom"] < tau:
        assert sm["adapt"] is True, sm
        assert sm["mode"]  == CANON[sm["state"]], sm
    else:
        assert sm["adapt"] is False, sm
        assert sm["mode"]  == 0, sm
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v197: non-deterministic output" >&2
    exit 1
fi
