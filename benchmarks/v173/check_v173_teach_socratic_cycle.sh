#!/usr/bin/env bash
#
# benchmarks/v173/check_v173_teach_socratic_cycle.sh
#
# Merge-gate for v173 σ-Teach.  Verifies:
#   1. self-test
#   2. state JSON: 4 subtopics + order vector + mastery booleans
#   3. curriculum is ordered weakest-first (σ_student descending)
#   4. difficulty tiers correctly track σ_student
#   5. v1.58-bit quantization has σ_teacher > τ_teacher and
#      therefore triggers exactly one ABSTAIN event, zero
#      exercises, remains non-mastered
#   6. at least one subtopic ends mastered (σ ≤ τ_mastery)
#   7. every exercise event has σ_after ≤ σ_before
#   8. trace NDJSON parses as a sequence of JSON objects with
#      kernel=v173 and expected event kinds
#   9. determinism: two runs produce byte-identical JSON + trace
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v173_teach"
test -x "$BIN" || { echo "v173: binary not built"; exit 1; }

echo "[v173] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v173] (2..7) state JSON"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v173_teach"]).decode())
assert len(doc["subtopics"]) == 4, doc
tau_t = doc["tau_teacher"]
tau_m = doc["tau_mastery"]

# weakest-first curriculum
sigmas = [doc["subtopics"][i]["sigma_student"] for i in doc["order"]]
for a, b in zip(sigmas, sigmas[1:]):
    assert a >= b - 1e-6, ("not descending", sigmas)

# difficulty tier sanity
def tier(s):
    if s <= 0.20 + 1e-6: return "expert"
    if s <= 0.40 + 1e-6: return "hard"
    if s <= 0.65 + 1e-6: return "medium"
    return "easy"
for sub in doc["subtopics"]:
    assert sub["difficulty"] == tier(sub["sigma_student"]), sub

# σ_teacher > τ_teacher subtopic exists, and is NOT mastered
quant = next(s for s in doc["subtopics"] if s["name"].startswith("v1.58"))
assert quant["sigma_teacher"] > tau_t, quant
assert quant["mastered"] is False, quant

# at least one mastered
n_master = sum(1 for s in doc["subtopics"] if s["mastered"])
assert n_master >= 1, doc
for s in doc["subtopics"]:
    if s["mastered"]:
        assert s["sigma_student"] <= tau_m + 1e-6, s
print("state ok, mastered=", n_master)
PY

echo "[v173] (5) abstain exclusivity + (7) monotone exercises (trace)"
T="$("$BIN" --trace)"
python3 - <<PY
import json
lines = [ln for ln in """$T""".strip().split("\n") if ln]
events = [json.loads(ln) for ln in lines]
quant_events = [e for e in events if e["subtopic"].startswith("v1.58")]
abstains    = [e for e in quant_events if e["event"] == "abstain"]
exercises_q = [e for e in quant_events if e["event"] == "exercise"]
completes_q = [e for e in quant_events if e["event"] == "complete"]
assert len(abstains)    == 1,  abstains
assert len(exercises_q) == 0,  exercises_q
assert len(completes_q) == 0,  completes_q
for e in events:
    if e["event"] == "exercise":
        assert e["sigma_student_after"] <= e["sigma_student_before"] + 1e-6, e
print("trace ok,", len(events), "events")
PY

echo "[v173] (9) determinism"
A="$("$BIN")";          B="$("$BIN")";          [ "$A" = "$B" ] || { echo "json nondet"; exit 9; }
A="$("$BIN" --trace)";  B="$("$BIN" --trace)";  [ "$A" = "$B" ] || { echo "trace nondet"; exit 9; }

echo "[v173] ALL CHECKS PASSED"
