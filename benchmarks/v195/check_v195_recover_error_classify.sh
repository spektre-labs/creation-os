#!/usr/bin/env bash
#
# v195 σ-Recover — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * all 5 error classes surfaced (HALLUCINATION /
#     CAPABILITY_GAP / PLANNING_ERROR / TOOL_FAILURE /
#     CONTEXT_LOSS) with ≥ 1 incident each
#   * every incident's recovery ops belong to the canonical set
#     for its class (class_to_ops_canonical == true)
#   * pass-1 consumes strictly fewer ops than pass-0 (learning)
#   * per-domain ECE strictly drops on every domain
#   * recovery log hash chain verifies
#   * byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v195_recover"
[ -x "$BIN" ] || { echo "v195: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]      == "v195", d
assert d["n_incidents"] == 30,      d
assert len(d["class_counts"]) == 5
for c in d["class_counts"]:
    assert c >= 1, d["class_counts"]

assert d["class_to_ops_canonical"] is True, d
assert d["chain_valid"]             is True, d
assert d["n_ops_pass1"] < d["n_ops_pass0"], d

# ECE strictly drops on every domain.
for i in range(4):
    assert d["ece_after"][i] < d["ece_before"][i], (i, d)

# Canonical op mapping (partition).
CANON = {
    0: {180, 125},     # hallucination → steer + dpo
    1: {141, 145},     # capability_gap → curriculum + skill
    2: {121},          # planning → replan
    3: {159},          # tool → heal
    4: {194},          # context → checkpoint
}
for inc in d["incidents"]:
    ops = {o for o in inc["ops"][:inc["n_ops"]]}
    assert ops and ops.issubset(CANON[inc["class"]]), inc

# Two passes, 15 incidents per pass.
p0 = [i for i in d["incidents"] if i["pass"] == 0]
p1 = [i for i in d["incidents"] if i["pass"] == 1]
assert len(p0) == 15 and len(p1) == 15, (len(p0), len(p1))
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v195: non-deterministic output" >&2
    exit 1
fi
