#!/usr/bin/env bash
#
# v196 σ-Habit — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * ≥ 3 habits compiled (is_habit == true)
#   * every habit: speedup ≥ 10× AND σ_steady < τ_break
#   * non-habits: either occurrences < τ_repeat, σ ≥ τ_break,
#     or speedup < 10× (never miscompiled)
#   * ≥ 1 break-out event in the trace (σ spike → cortex)
#   * steady-state ticks run in cerebellum (mode == 1)
#   * habit library hash chain valid
#   * byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v196_habit"
[ -x "$BIN" ] || { echo "v196: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"] == "v196", d
assert d["n_patterns"] == 8, d
assert d["n_habits"]   >= 3, d
assert d["tau_repeat"] == 5.0, d
assert d["min_speedup"] == 10.0, d
assert d["chain_valid"] is True, d

for p in d["patterns"]:
    if p["is_habit"]:
        assert p["speedup"]      >= 10.0, p
        assert p["sigma_steady"] <  d["tau_break"], p
        assert p["occ"]          >= d["tau_repeat"], p
    else:
        bad = (
            p["occ"]          < d["tau_repeat"]
            or p["sigma_steady"] >= d["tau_break"]
            or p["speedup"]      < 10.0
        )
        assert bad, p

cortex      = [t for t in d["trace"] if t["mode"] == 0]
cerebellum  = [t for t in d["trace"] if t["mode"] == 1]
break_outs  = [t for t in d["trace"] if t["break_out"]]

assert len(break_outs) >= 1, "no break-out"
for t in break_outs:
    assert t["mode"] == 0, t
    assert t["sigma"] >= d["tau_break"], t
for t in cerebellum:
    assert t["sigma"] < d["tau_break"], t
    assert t["break_out"] is False, t

# Speedup empirically: sum of cerebellum cycles much less than
# the equivalent reasoning path.
ncerb_cycles  = sum(t["cycles"] for t in cerebellum)
equiv_cycles  = 0
# For each cerebellum tick, its equivalent full-reasoning cost is
# d["patterns"][t["pid"]]["cycles_reasoning"].
for t in cerebellum:
    equiv_cycles += d["patterns"][t["pid"]]["cycles_reasoning"]
assert equiv_cycles >= 10 * ncerb_cycles, (equiv_cycles, ncerb_cycles)
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v196: non-deterministic output" >&2
    exit 1
fi
