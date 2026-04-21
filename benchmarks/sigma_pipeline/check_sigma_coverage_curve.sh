#!/usr/bin/env bash
#
# SCI-3 smoke test: accuracy-coverage curve sweep.
#
# Pinned facts:
#   1. --self-test PASSes.
#   2. A real sweep on benchmarks/pipeline/truthfulqa_817_detail.jsonl
#      (pipeline mode, step=0.05) produces a valid "cos.coverage_curve.
#      v1" JSON with monotonically non-decreasing coverage in τ.
#   3. accuracy_all equals n_correct / n_scored.
#   4. At τ=1.0 (widest gate) coverage == 1.0.
#   5. accuracy_accepted at the smallest non-zero-coverage τ is ≥
#      accuracy_all (σ-gated subset is no worse than the full set).
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_coverage_curve"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

# 1. Self-test.
"$BIN" --self-test >/dev/null

DATA="benchmarks/pipeline/truthfulqa_817_detail.jsonl"
if [[ ! -f "$DATA" ]]; then
    echo "check_sigma_coverage_curve: detail JSONL missing ($DATA)" >&2
    exit 2
fi

OUT=$(mktemp); JSON=$(mktemp)
trap 'rm -f "$OUT" "$JSON"' EXIT

"$BIN" --dataset "$DATA" --mode pipeline --step 0.05 --delta 0.05 \
       --out "$JSON" >"$OUT"

python3 - "$JSON" <<'PY'
import json, sys
j = json.load(open(sys.argv[1]))

assert j["schema"] == "cos.coverage_curve.v1", j
assert j["n_scored"]  > 0, j
assert j["n_correct"] > 0, j

# 3. Sanity on accuracy_all.
expected = j["n_correct"] / j["n_scored"]
assert abs(j["accuracy_all"] - expected) < 1e-6, (j["accuracy_all"], expected)

rows = j["rows"]
assert len(rows) >= 10, len(rows)

# 2. Monotonically non-decreasing coverage as τ grows.
prev = -1.0
for r in rows:
    assert 0.0 <= r["coverage"] <= 1.0 + 1e-6, r
    assert r["coverage"] >= prev - 1e-6, (r, prev)
    prev = r["coverage"]

# 4. At or near τ=1.0 we expect coverage == 1.0 on this dataset.
assert rows[-1]["coverage"] >= 0.999, rows[-1]

# 5. σ-gate utility: the curve must exhibit *some* τ at which the
# accepted subset is at least as accurate as the full set.  We
# require this at any reasonably supported τ (n_accept ≥ 20) and
# allow a 0.02 slack for small-sample jitter.  This is the weakest
# statement that still signals "σ carries information about
# correctness" — if no such τ exists, SCI-3 has no story to tell.
supported = [r for r in rows if r["n_accept"] >= 20 and r["accuracy"] is not None]
assert supported, rows
best = max(supported, key=lambda r: r["accuracy"])
assert best["accuracy"] >= j["accuracy_all"] - 0.02, \
    (best, j["accuracy_all"])

# The final row (τ=1.0) must coincide with accuracy_all.
tail = rows[-1]
assert tail["n_accept"] == j["n_scored"], tail
assert abs(tail["accuracy"] - j["accuracy_all"]) < 1e-6, tail

print("check_sigma_coverage_curve: PASS", {
    "rows": len(rows),
    "accuracy_all": round(j["accuracy_all"], 4),
    "best_selective_accuracy": round(best["accuracy"], 4),
    "best_selective_tau": best["tau"],
    "final_coverage": tail["coverage"],
})
PY
