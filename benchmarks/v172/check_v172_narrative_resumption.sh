#!/usr/bin/env bash
#
# benchmarks/v172/check_v172_narrative_resumption.sh
#
# Merge-gate for v172 σ-Narrative.  Verifies:
#   1. self-test
#   2. fixture: 3 sessions × 4 facts, 3 goals, 4 people
#   3. σ_coverage ≤ τ_coverage on every session (quality summaries)
#   4. narrative thread includes every session label
#   5. resume line references last session AND primary goal
#   6. people lookup: lauri=owner, topias=editor @session 2
#   7. primary goal selector = lowest σ_progress among non-done goals
#   8. JSON + thread determinism
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v172_narrative"
test -x "$BIN" || { echo "v172: binary not built"; exit 1; }

echo "[v172] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v172] (2..3, 6..7) state JSON"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v172_narrative"]).decode())
assert doc["n_sessions"] == 3, doc
assert doc["n_goals"]    == 3, doc
assert doc["n_people"]   == 4, doc
tau = doc["tau_coverage"]
for sess in doc["sessions"]:
    assert sess["n_facts"] == 4, sess
    assert 0.0 <= sess["sigma_coverage"] <= tau + 1e-6, sess
names = {p["name"]: p for p in doc["people"]}
assert names["lauri"]["role"]  == "owner"
assert names["topias"]["role"] == "editor"
assert names["topias"]["last_session_id"] == 2
# primary goal selector: lowest σ_progress among non-done
open_goals = [g for g in doc["goals"] if not g["done"]]
primary = min(open_goals, key=lambda g: g["sigma_progress"])
assert primary["name"] in doc["resume"], (primary["name"], doc["resume"])
print("state ok, primary goal=", primary["name"])
PY

echo "[v172] (4) narrative thread"
T="$("$BIN" --thread)"
echo "$T" | grep -q "week 1" || { echo "thread missing week 1"; exit 4; }
echo "$T" | grep -q "week 2" || { echo "thread missing week 2"; exit 4; }
echo "$T" | grep -q "week 3" || { echo "thread missing week 3"; exit 4; }

echo "[v172] (5) resume line"
R="$("$BIN" --resume)"
echo "$R" | grep -q "week 3"                     || { echo "resume: no last session"; exit 5; }
echo "$R" | grep -q "Creation OS v1.0 release"   || { echo "resume: no primary goal"; exit 5; }
echo "$R" | grep -qi "awaiting instructions"     || { echo "resume: no protocol tail"; exit 5; }

echo "[v172] (8) determinism"
A="$("$BIN")";            B="$("$BIN")";            [ "$A" = "$B" ] || { echo "json nondet"; exit 8; }
A="$("$BIN" --thread)";   B="$("$BIN" --thread)";   [ "$A" = "$B" ] || { echo "thread nondet"; exit 8; }
A="$("$BIN" --resume)";   B="$("$BIN" --resume)";   [ "$A" = "$B" ] || { echo "resume nondet"; exit 8; }

echo "[v172] ALL CHECKS PASSED"
