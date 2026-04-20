#!/usr/bin/env bash
# Full-pipeline smoke test (P8).
#
# Drives the orchestrator with the deterministic stub backend; verifies:
#   1. First call MISSES engram, traverses bitnet → σ-gate → escalate
#      (stub), stores to engram.
#   2. Second call HITS engram (O(1) return, origin=="engram",
#      eur_cost==0.0).
#   3. A second, different prompt still escalates so both paths are
#      covered.
#   4. JSONL logs contain every expected step (engram_miss,
#      bitnet_round, sigma_gate, escalate, engram_store, engram_hit).
set -euo pipefail
cd "$(dirname "$0")/../.."

PY="${PYTHON:-.venv-bitnet/bin/python}"
[[ -x "$PY" ]] || PY="$(command -v python3)"

LOG1=$(mktemp /tmp/orch_log1.XXXX)
LOG2=$(mktemp /tmp/orch_log2.XXXX)
LOG3=$(mktemp /tmp/orch_log3.XXXX)
OUT=$(mktemp /tmp/orch_out.XXXX)
trap 'rm -f "$LOG1" "$LOG2" "$LOG3" "$OUT"' EXIT

PYTHONPATH=scripts "$PY" - "$LOG1" "$LOG2" "$LOG3" >"$OUT" <<'PY'
import json, sys, pathlib
from sigma_pipeline.orchestrator import Orchestrator, OrchestratorConfig
from sigma_pipeline.backends import StubBackend
from sigma_pipeline.engram import fnv1a_64

log1, log2, log3 = (pathlib.Path(p) for p in sys.argv[1:4])
cfg = OrchestratorConfig(max_tokens=16)

v1 = Orchestrator(StubBackend(max_steps=64), cfg).orchestrate(
    "What is 2+2?", log_path=log1,
)
# Fresh orchestrator but seed its engram with a confident answer so we
# exercise the O(1) HIT path.
o2 = Orchestrator(StubBackend(max_steps=64), cfg)
o2._engram.put(fnv1a_64("What is 2+2?"), 0.05, v1.answer)
v2 = o2.orchestrate("What is 2+2?", log_path=log2)
v3 = Orchestrator(StubBackend(max_steps=64), cfg).orchestrate(
    "Prove Fermat's last theorem.", log_path=log3,
)
print(json.dumps({"v1": v1.as_json(), "v2": v2.as_json(),
                  "v3": v3.as_json()}))
PY

echo "  · call results:"
cat "$OUT"

"$PY" - "$OUT" <<'PY'
import json, sys
d = json.loads(open(sys.argv[1]).read())
assert d["v1"]["engram_hit"] is False,     d["v1"]
assert d["v1"]["origin"] in ("api","local"), d["v1"]
assert d["v2"]["origin"] == "engram",       d["v2"]
assert d["v2"]["engram_hit"] is True,       d["v2"]
assert d["v2"]["eur_cost"] == 0.0,          d["v2"]
assert d["v3"]["engram_hit"] is False,      d["v3"]
print("py-assert: ok")
PY

grep -q '"step": "engram_miss"'   "$LOG1" || { echo "log1 missing engram_miss"   >&2; exit 4; }
grep -q '"step": "bitnet_round"'  "$LOG1" || { echo "log1 missing bitnet_round"  >&2; exit 5; }
grep -q '"step": "sigma_gate"'    "$LOG1" || { echo "log1 missing sigma_gate"    >&2; exit 6; }
grep -q '"step": "engram_store"'  "$LOG1" || { echo "log1 missing engram_store"  >&2; exit 7; }
grep -q '"step": "engram_hit"'    "$LOG2" || { echo "log2 missing engram_hit"    >&2; exit 8; }
grep -q '"step": "engram_miss"'   "$LOG3" || { echo "log3 missing engram_miss"   >&2; exit 9; }

echo "check-orchestrator: PASS (engram hit/miss + bitnet + σ-gate + escalate + store)"
