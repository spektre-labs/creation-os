#!/usr/bin/env bash
#
# v194 σ-Horizon — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * goal tree has 1 strategic / 3 tactical / 12 operational
#   * degradation detected on the 30-tick trajectory
#   * recovery ladder runs in order: FLUSH(1) → SUMMARIZE(2) → BREAK(3)
#   * σ after the full ladder is strictly lower than at detection
#   * checkpoint hash chain verifies
#   * simulated crash recovery reproduces the terminal hash
#   * byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v194_horizon"
[ -x "$BIN" ] || { echo "v194: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]        == "v194", d
assert d["n_strategic"]   == 1,      d
assert d["n_tactical"]    == 3,      d
assert d["n_operational"] == 12,     d
assert d["n_ticks"]       == 30,     d

assert d["degradation_detected"]   is True, d
assert d["detection_tick"]         >  0,    d
assert d["recovery_steps_run"]     == 3,    d
assert d["recovery_ladder"]        == [1, 2, 3], d
assert d["checkpoint_chain_valid"] is True, d
assert d["crash_recovery_matches"] is True, d

# σ after ladder strictly lower than at detection tick.
dt = d["detection_tick"]
assert d["steps"][dt + 3]["sigma_after"] < d["steps"][dt]["sigma"], d

# Exactly one step carries degradation=true (the detection tick).
flagged = [s for s in d["steps"] if s["degradation"]]
assert len(flagged) == 1, flagged
assert flagged[0]["t"] == dt, flagged

# Tree sanity.
strategic = [g for g in d["goals"] if g["tier"] == 0]
tactical  = [g for g in d["goals"] if g["tier"] == 1]
ops       = [g for g in d["goals"] if g["tier"] == 2]
assert len(strategic) == 1 and len(tactical) == 3 and len(ops) == 12
for g in tactical:
    assert g["parent"] == 0
for g in ops:
    assert 1 <= g["parent"] <= 3
    assert g["checkpoint_hash"] != "0000000000000000", g
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v194: non-deterministic output" >&2
    exit 1
fi
