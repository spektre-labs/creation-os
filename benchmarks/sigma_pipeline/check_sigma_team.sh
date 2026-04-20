#!/usr/bin/env bash
#
# HERMES-2 smoke test: σ-multi-agent orchestration.
#
# Pinned facts (per team_main.c five-role roster):
#   self_test    == 0
#   run_rc       == 0
#   sigma_goal   <= tau_abort
#   escalations  == 0
#   steps roles  == [researcher, coordinator, coder, reviewer, writer]
#   step 0 assigned == "rosa" (capability 0.90 > 0.60)
#   step 2 assigned == "cody" (capability 0.88 > 0.70)
#   Every step's σ <= tau_abort
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_team"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_team: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_team", doc
assert doc["self_test"] == 0, doc
assert doc["run_rc"] == 0, doc
assert doc["escalations"] == 0, doc
assert doc["sigma_goal"] <= doc["tau_abort"], doc

steps = doc["steps"]
assert len(steps) == 5, steps
assert [s["role"] for s in steps] == \
    ["researcher", "coordinator", "coder", "reviewer", "writer"], steps

for s in steps:
    assert 0.0 <= s["sigma"] <= 1.0, s
    assert s["sigma"] <= doc["tau_abort"], s
    assert s["attempts"] >= 1, s
    assert s["escalated"] is False, s
    assert s["assigned"] != "", s

# Highest-capability agent wins on ties between two researchers / coders.
assert steps[0]["assigned"] == "rosa", steps[0]    # 0.90 > 0.60
assert steps[2]["assigned"] == "cody", steps[2]    # 0.88 > 0.70
assert steps[3]["assigned"] == "rex",  steps[3]
assert steps[4]["assigned"] == "willow", steps[4]

# Roster sanity: used agents are exactly the ones assigned to some step.
used = {a["id"] for a in doc["roster"] if a["uses"] > 0}
assigned = {s["assigned"] for s in steps}
assert used == assigned, (used, assigned)

print("check_sigma_team: PASS", {
    "sigma_goal": doc["sigma_goal"],
    "steps": len(steps),
    "escalations": doc["escalations"],
})
PY
