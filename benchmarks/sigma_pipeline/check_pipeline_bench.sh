#!/usr/bin/env bash
# End-to-end pipeline benchmark smoke test (P9).
#
# Runs pipeline_bench over the built-in deterministic DEMO_FIXTURE
# (20 rows) and pins the headline numbers so any semantic drift in
# reinforce / speculative / ttt / engram / generate_until / orchestrator
# shows up here.
set -euo pipefail
cd "$(dirname "$0")/../.."

PY="${PYTHON:-.venv-bitnet/bin/python}"
[[ -x "$PY" ]] || PY="$(command -v python3)"

MD=$(mktemp /tmp/pb_md.XXXX).md
JSON=$(mktemp /tmp/pb_sum.XXXX).json
trap 'rm -f "$MD" "$JSON"' EXIT

PYTHONPATH=scripts "$PY" -m sigma_pipeline.pipeline_bench \
    --output-md "$MD" --output-json "$JSON"

echo "  · markdown:"
sed 's/^/    /' "$MD"
echo "  · summary: $(cat "$JSON")"

"$PY" - "$JSON" <<'PY'
import json, sys, math
s = json.loads(open(sys.argv[1]).read())
# The DEMO_FIXTURE has 20 rows and is deterministic.  Pin the exact
# counts and headline fractions.
def eq(a, b, tol=1e-9):
    return math.isclose(a, b, rel_tol=tol, abs_tol=tol)

assert s["n"] == 20,                      s
assert s["n_engram"] + s["n_local"] + s["n_api"] == 20, s
assert s["n_engram"] >= 10,               s          # engram warms up
assert s["n_api"] >= 5,                   s          # spikes escalate
assert s["n_abstained"] == 0,             s          # clean fixture
assert eq(s["accuracy_api"],    1.0),     s
assert eq(s["accuracy_hybrid"], 1.0),     s
assert eq(s["accuracy_retained"], 1.0),   s
assert 0.50 <= s["savings_frac"] <= 0.95, s          # hybrid saves
assert 0.50 <= s["local_share"]  <= 0.95, s          # mostly local
print("py-assert: ok  "
      f"(n_engram={s['n_engram']}  n_api={s['n_api']}  "
      f"savings={s['savings_frac']:.2f}  retained="
      f"{s['accuracy_retained']:.2f})")
PY

grep -q "Creation OS served" "$MD" || { echo "md missing headline" >&2; exit 4; }

echo "check-pipeline-bench: PASS (20-row demo: hybrid retains 100% accuracy, saves ≥50%)"
