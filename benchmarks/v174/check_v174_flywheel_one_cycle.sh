#!/usr/bin/env bash
#
# benchmarks/v174/check_v174_flywheel_one_cycle.sh
#
# Merge-gate for v174 σ-Flywheel.  Verifies:
#   1. self-test
#   2. one cycle: n_chosen + n_pair + n_skip == 50
#   3. all 3 classes exercised (n > 0 each)
#   4. all 5 clusters hit (distinct_clusters == 5)
#   5. entropy > h_min AND σ-variance > var_min → no collapse
#   6. DPO NDJSON: every line has valid class ∈ {chosen, pair},
#      no SKIP lines leak
#   7. regression guard: cycle with baseline=0.99 triggers
#      collapse with a reason containing "baseline"
#   8. determinism of JSON and DPO
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v174_flywheel"
test -x "$BIN" || { echo "v174: binary not built"; exit 1; }

echo "[v174] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v174] (2..5) one cycle"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v174_flywheel"]).decode())
assert doc["n_samples"] == 50, doc
assert doc["n_chosen"] + doc["n_pair"] + doc["n_skip"] == 50, doc
assert doc["n_chosen"] > 0, doc
assert doc["n_pair"]   > 0, doc
assert doc["n_skip"]   > 0, doc
assert doc["distinct_clusters"] == 5, doc
assert doc["entropy"]        > doc["h_min"],  doc
assert doc["sigma_variance"] > doc["var_min"], doc
assert doc["collapse_triggered"] is False, doc
print("cycle ok, chosen/pair/skip=%d/%d/%d H=%.2f var=%.3f score=%.2f" % (
    doc["n_chosen"], doc["n_pair"], doc["n_skip"],
    doc["entropy"], doc["sigma_variance"], doc["benchmark_score"]))
PY

echo "[v174] (6) DPO NDJSON"
D="$("$BIN" --dpo)"
python3 - <<PY
ndjson = """$D"""
lines = [ln for ln in ndjson.strip().split("\n") if ln]
assert len(lines) > 0
import json
n_chosen = 0; n_pair = 0
for ln in lines:
    r = json.loads(ln)
    assert r["kernel"] == "v174"
    assert r["class"] in ("chosen", "pair"), r
    if r["class"] == "chosen":
        n_chosen += 1
        assert "sigma_answer" in r
    else:
        n_pair += 1
        assert "sigma_rejected" in r and "sigma_chosen" in r
        assert r["sigma_chosen"] < r["sigma_rejected"], r
assert n_chosen > 0 and n_pair > 0, (n_chosen, n_pair)
print("dpo ok, chosen=%d pair=%d (no skip leaked)" % (n_chosen, n_pair))
PY

echo "[v174] (7) regression guard"
R="$("$BIN" --baseline 0.99)" || true
python3 - <<PY
import json
doc = json.loads("""$R""")
assert doc["collapse_triggered"] is True, doc
assert "baseline" in doc["collapse_reason"] or "score" in doc["collapse_reason"], doc
print("regression guard ok:", doc["collapse_reason"])
PY

echo "[v174] (8) determinism"
A="$("$BIN")";       B="$("$BIN")";       [ "$A" = "$B" ] || { echo "json nondet"; exit 8; }
A="$("$BIN" --dpo)"; B="$("$BIN" --dpo)"; [ "$A" = "$B" ] || { echo "dpo nondet";  exit 8; }

echo "[v174] ALL CHECKS PASSED"
