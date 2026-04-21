#!/usr/bin/env bash
#
# SCI-1 smoke test: conformal σ-calibration.
#
# Pinned facts:
#   1. cos-calibrate --self-test exits 0 (synthetic monotone dataset).
#   2. A real calibration against benchmarks/pipeline/
#      truthfulqa_817_detail.jsonl at α=0.30, δ=0.10 produces a bundle
#      with at least one report; "all" is present; every report's
#      risk_ucb ≤ α when valid=1.
#   3. Tighter α yields τ ≤ looser α's τ (monotonicity of the risk-
#      controlling threshold).
#   4. Output file is valid JSON and contains the "cos.conformal.v1"
#      schema tag.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_conformal"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

# 1. Self-test.
"$BIN" --self-test >/dev/null

# 2. & 4. Real calibration.
DATA="benchmarks/pipeline/truthfulqa_817_detail.jsonl"
if [[ ! -f "$DATA" ]]; then
    echo "check_sigma_conformal: detail JSONL missing ($DATA)" >&2
    exit 2
fi
OUT1=$(mktemp); OUT2=$(mktemp); JSON1=$(mktemp); JSON2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2" "$JSON1" "$JSON2"' EXIT

"$BIN" --dataset "$DATA" --mode pipeline --alpha 0.30 --delta 0.10 \
       --out "$JSON1" >"$OUT1"
"$BIN" --dataset "$DATA" --mode pipeline --alpha 0.60 --delta 0.10 \
       --out "$JSON2" >"$OUT2"

python3 - "$JSON1" "$JSON2" <<'PY'
import json, sys
j1 = json.load(open(sys.argv[1]))
j2 = json.load(open(sys.argv[2]))

for j, alpha in ((j1, 0.30), (j2, 0.60)):
    assert j["schema"] == "cos.conformal.v1", j
    assert abs(j["alpha"] - alpha) < 1e-6, j
    rs = j["per_domain"]
    assert len(rs) >= 1, j
    domains = [r["domain"] for r in rs]
    assert domains[0] == "all", domains
    for r in rs:
        if r["valid"]:
            assert r["risk_ucb"] <= alpha + 1e-6, (r, alpha)
            assert 0.0 <= r["coverage"] <= 1.0, r

# 3. Monotonicity on the "all" row.
tau_tight = j1["per_domain"][0]["tau"]
tau_loose = j2["per_domain"][0]["tau"]
assert tau_tight <= tau_loose + 1e-6, (tau_tight, tau_loose)

print("check_sigma_conformal: PASS", {
    "tau_alpha_0.30": tau_tight,
    "tau_alpha_0.60": tau_loose,
    "domains": len(j1["per_domain"]),
})
PY
