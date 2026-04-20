#!/usr/bin/env bash
#
# HERMES-3 smoke test: σ-benchmark-suite + regression gate.
#
# Pinned facts (per bench_suite_main.c):
#   self_test == 0
#   5 rows: truthfulqa, arc_challenge, arc_easy, hellaswag, mmlu_top7
#   weighted_sigma in (0, 1)
#   clean_run.rc == 0, clean_run.regressions == 0
#   injected_regression.rc == COS_BENCH_REGRESSION (-4)
#   injected_regression.regressions == 3 (accuracy + sigma_mean + escalation_rate)
#   All regression rows target "truthfulqa"
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_suite"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_suite: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_bench_suite", doc
assert doc["self_test"] == 0, doc

rows = doc["rows"]
names = [r["name"] for r in rows]
assert names == ["truthfulqa", "arc_challenge", "arc_easy",
                 "hellaswag", "mmlu_top7"], names

for r in rows:
    assert r["n_questions"] > 0, r
    for k in ("accuracy", "accuracy_accepted", "coverage",
              "sigma_mean", "rethink_rate", "escalation_rate"):
        assert 0.0 <= r[k] <= 1.0, (r, k)
    assert r["cost_eur"] > 0.0, r

assert 0.0 < doc["weighted_sigma"] < 1.0, doc
assert 0.0 < doc["mean_accuracy"]  < 1.0, doc
assert doc["total_cost_eur"] > 0.0, doc

clean = doc["clean_run"]
assert clean["rc"] == 0 and clean["regressions"] == 0, clean

bad = doc["injected_regression"]
assert bad["rc"] == -4, bad                   # COS_BENCH_REGRESSION
assert bad["regressions"] == 3, bad
metrics = sorted(d["metric"] for d in bad["details"])
assert metrics == ["accuracy", "escalation_rate", "sigma_mean"], metrics
for d in bad["details"]:
    assert d["benchmark"] == "truthfulqa", d

print("check_sigma_suite: PASS", {
    "rows": len(rows),
    "weighted_sigma": doc["weighted_sigma"],
    "regressions_found": bad["regressions"],
})
PY
