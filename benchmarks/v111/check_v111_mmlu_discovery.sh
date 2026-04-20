#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v111.2 — merge-gate smoke for MMLU floor-check / σ-matrix artifacts.
#
# Does NOT re-run BitNet (that's a ≥30 min job).  Only checks that the
# discovery + σ-matrix artifacts are present, well-formed JSON, and
# carry the invariants iteration-3 requires:
#
#   - mmlu_discovery.json exists and names an `accuracy_floor` of 0.40
#   - mmlu_discovery.json carries the `sigma_analysis_eligible` key
#   - mmlu_subset.json exists, is flagged `preregistered = false`,
#     and uses Bonferroni N = 4 × n_subjects (not 3 × n_subjects)
#
# If the discovery / subset artifacts are missing, this script EXITS 0
# with a clear reason — we do NOT fail CI on hosts where BitNet has not
# been wired.  The merge-gate failure mode is "artifacts are present
# but malformed", not "artifacts are missing".
set -euo pipefail
cd "$(dirname "$0")/../.."

RESULTS="benchmarks/v111/results"
DISC="$RESULTS/mmlu_discovery.json"
MATRIX="$RESULTS/mmlu_subset.json"

say() { echo "check-v111-mmlu-discovery: $*"; }

if [ ! -f "$DISC" ]; then
    say "SKIP — $DISC not found.  Run "\
         "'.venv-bitnet/bin/python benchmarks/v111/mmlu_subject_discovery.py' first."
    exit 0
fi

PY=".venv-bitnet/bin/python"
[ -x "$PY" ] || PY="python3"

"$PY" - <<PYCHECK
import json, sys
with open("$DISC") as f:
    d = json.load(f)
assert "accuracy_floor" in d, "mmlu_discovery missing accuracy_floor"
assert d["accuracy_floor"] == 0.40, \
    f"expected accuracy_floor=0.40, got {d['accuracy_floor']}"
assert "sigma_analysis_eligible" in d, \
    "mmlu_discovery missing sigma_analysis_eligible"
assert "subjects" in d and isinstance(d["subjects"], list), \
    "mmlu_discovery missing subjects[]"
# Every subject row has {task, acc (float or null), passed_40_floor}.
for s in d["subjects"]:
    for key in ("task", "passed_40_floor"):
        assert key in s, f"subject row missing '{key}': {s}"
print(f"  · discovery: n_subjects={len(d['subjects'])}  "
      f"eligible={len(d['sigma_analysis_eligible'])}")
PYCHECK

if [ ! -f "$MATRIX" ]; then
    say "OK — discovery well-formed; σ-matrix ($MATRIX) not yet run."
    exit 0
fi

"$PY" - <<PYCHECK
import json, sys
with open("$MATRIX") as f:
    m = json.load(f)
assert m.get("preregistered") is False, \
    "mmlu_subset.json must mark preregistered=false (post-hoc)"
N = m.get("bonferroni_N", 0)
tasks = m.get("tasks", [])
n_scored = sum(1 for t in tasks if not t.get("skipped"))
if n_scored > 0:
    # iteration-3 requires 4 non-entropy signals in the pool.
    assert N == 4 * n_scored, (
        f"bonferroni_N = {N} does not match 4 x n_scored({n_scored});"
        " iteration-3 requires 4 non-entropy signals in the pool "
        "(sigma_max_token, sigma_product, sigma_composite_max, "
        "sigma_composite_mean).")
print(f"  · subset: n_scored={n_scored}  bonferroni_N={N}")
PYCHECK

say "OK — MMLU discovery + σ-matrix artifacts well-formed."
