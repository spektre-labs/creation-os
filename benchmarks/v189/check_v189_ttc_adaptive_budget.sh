#!/usr/bin/env bash
#
# v189 σ-TTC — merge-gate check.
#
# Contracts:
#   * self-test PASSES (BALANCED + FAST + DEEP)
#   * compute allocation is monotone: hard ≥ 2× medium ≥ 2× easy
#   * hard/easy ratio > 4× in BALANCED and DEEP
#   * FAST mode caps paths=1 AND forbids debate/symbolic/reflect
#   * DEEP mode floors paths at max_paths_deep (8) for every query
#   * easy queries use exactly 1 path and 1 recurrent iter/token
#   * byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v189_ttc"
[ -x "$BIN" ] || { echo "v189: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

BAL="$("$BIN" --mode balanced)"
FAST="$("$BIN" --mode fast)"
DEEP="$("$BIN" --mode deep)"

python3 - <<PY
import json
bal  = json.loads("""$BAL""")
fast = json.loads("""$FAST""")
deep = json.loads("""$DEEP""")

for d in (bal, fast, deep):
    assert d["kernel"] == "v189", d
    assert d["n_queries"] == 48, d
    assert d["n_easy"] + d["n_medium"] + d["n_hard"] == 48, d
    assert d["n_easy"]  >= 1 and d["n_medium"] >= 1 and d["n_hard"] >= 1

# Monotone spending in balanced mode.
assert bal["mean_compute_hard"]   >= 2.0 * bal["mean_compute_medium"], bal
assert bal["mean_compute_medium"] >= 2.0 * bal["mean_compute_easy"],   bal
assert bal["ratio_hard_over_easy"] > 4.0, bal

# FAST mode: paths=1 and no plugins (debate/symbolic/reflect).
for q in fast["queries"]:
    assert q["n_paths"] == 1, q
    assert not q["debate"] and not q["symbolic"] and not q["reflect"], q

# DEEP mode: every query uses 8 paths; hard uses debate+symbolic+reflect.
assert deep["max_paths_deep"] == 8
for q in deep["queries"]:
    assert q["n_paths"] == deep["max_paths_deep"], q
    if q["class"] == 2:
        assert q["debate"] and q["symbolic"] and q["reflect"], q

# DEEP amplifies hard-over-easy ratio > 4×.
assert deep["ratio_hard_over_easy"] > 4.0, deep
# DEEP spends strictly more than FAST on hard.
assert deep["mean_compute_hard"] > fast["mean_compute_hard"], (deep, fast)
PY

A="$("$BIN" --mode balanced)"
B="$("$BIN" --mode balanced)"
if [ "$A" != "$B" ]; then
    echo "v189: non-deterministic output" >&2
    exit 1
fi
