#!/usr/bin/env bash
#
# v236 σ-Autobiography — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_milestones == 8
#   * ticks strictly ascending; sigma ∈ [0, 1]; every milestone
#     is consistent_with_prev == true (Prolog ground truth)
#   * sigma_autobiography == 1.0 (±1e-6)
#   * first_tick == milestones[0].tick; last_tick == milestones[-1].tick
#   * strongest_domain and weakest_domain non-empty and distinct;
#     strongest has the lowest mean σ across its milestones
#   * narrative is non-empty ASCII
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v236_autobiography"
[ -x "$BIN" ] || { echo "v236: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json, statistics
d = json.loads("""$OUT""")
assert d["kernel"] == "v236", d
assert d["chain_valid"] is True, d
assert d["n_milestones"] == 8, d

prev_tick = -1
for m in d["milestones"]:
    assert 0.0 <= m["sigma"] <= 1.0, m
    assert m["tick"] > prev_tick, (m, prev_tick)
    prev_tick = m["tick"]
    assert m["consistent_with_prev"] is True, m
    assert m["domain"] and m["content"], m

assert d["first_tick"] == d["milestones"][0]["tick"], d
assert d["last_tick"]  == d["milestones"][-1]["tick"], d
assert abs(d["sigma_autobiography"] - 1.0) < 1e-6, d
assert d["strongest_domain"] and d["weakest_domain"], d
assert d["strongest_domain"] != d["weakest_domain"], d

by_domain = {}
for m in d["milestones"]:
    by_domain.setdefault(m["domain"], []).append(m["sigma"])
means = {k: sum(v) / len(v) for k, v in by_domain.items()}
strongest = min(means, key=lambda k: (means[k], k))
weakest   = max(means, key=lambda k: (means[k], -ord(k[0])))
assert d["strongest_domain"] == strongest, (d["strongest_domain"], means)

assert isinstance(d["narrative"], str) and d["narrative"], d
assert all(0x20 <= ord(c) < 0x7F for c in d["narrative"]), d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v236: non-deterministic" >&2; exit 1; }
