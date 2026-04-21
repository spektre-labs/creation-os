#!/usr/bin/env bash
#
# SCI-6 smoke test: multi-dataset σ-gate aggregator.
#
# Pinned facts:
#   1. --self-test PASSes.
#   2. Default manifest eval writes benchmarks/suite/full_results.json
#      with schema "cos.suite_sci.v1".
#   3. At least one dataset is measured (TruthfulQA ships in-repo);
#      any measured row has n_scored > 0.  When ARC/GSM8K/HellaSwag
#      detail JSONLs are added, their rows flip to measured=true.
#   4. Every row's numeric fields are in [0,1] or sentinel 0.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_suite_sci"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

# 1. Self-test.
"$BIN" --self-test >/dev/null

# 2. Default manifest eval.
OUT=$(mktemp); JSON=$(mktemp)
trap 'rm -f "$OUT" "$JSON"' EXIT
"$BIN" --alpha 0.60 --delta 0.10 --out "$JSON" >"$OUT"

python3 - "$JSON" <<'PY'
import json, sys
j = json.load(open(sys.argv[1]))

assert j["schema"] == "cos.suite_sci.v1", j
rows = j["rows"]
assert len(rows) >= 2, rows

names = {r["name"] for r in rows}
assert "truthfulqa" in names, names

for r in rows:
    for k in ("accuracy_all", "accuracy_accepted",
              "coverage_scored", "coverage_accepted",
              "sigma_mean", "tau", "alpha", "delta", "risk_ucb"):
        v = r[k]
        assert isinstance(v, (int, float)), (r, k)
        assert 0.0 <= v <= 1.0 + 1e-6, (r, k)

tqa = next(r for r in rows if r["name"] == "truthfulqa")
assert tqa["measured"] == 1, tqa
assert tqa["n_scored"] > 0, tqa

measured = [r for r in rows if r["measured"] == 1]
assert measured, rows

for r in measured:
    assert r["n_rows"] > 0, r
    assert r["n_scored"] > 0, r

print("check_sigma_suite_sci: PASS", {
    "rows": len(rows),
    "measured": len(measured),
    "truthfulqa_acc_all": round(tqa["accuracy_all"], 4),
    "truthfulqa_acc_accepted": round(tqa["accuracy_accepted"], 4),
    "truthfulqa_tau_valid": tqa["tau_valid"],
})
PY
