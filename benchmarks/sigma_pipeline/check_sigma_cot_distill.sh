#!/usr/bin/env bash
#
# NEXT-2 smoke test: σ-CoT-distill — chain-of-thought distillation
# with per-step σ.
#
# Pinned facts on the two-pair canonical demo (τ_rethink = 0.50,
# τ_confident = 0.20):
#   - self_test_rc == 0
#   - total_seen   == 2
#   - rethink_points_total == 2 (one rethink per pair, at step 3)
#   - pair 1: n_steps=5, n_rethink=1, σ_max_step=0.6500
#   - pair 2: n_steps=4, n_rethink=1, σ_max_step=0.5200
#   - σ_cot_value > 0 (teacher paused and rethought)
#   - rethink indices are *inside* the step range (never > n_steps)
#
# Deterministic: two runs produce byte-identical JSON.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_cot_distill"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT_A="$($BIN)"
OUT_B="$($BIN)"

[[ "$OUT_A" == "$OUT_B" ]] \
    || { echo "FAIL: cot-distill output not deterministic" >&2; exit 3; }

for fact in \
    '"self_test_rc":0' \
    '"total_seen":2' \
    '"rethink_points_total":2' \
    '"pair":1,"n_steps":5,"n_rethink":1,"sigma_cot_value":0.0900,"sigma_max_step":0.6500' \
    '"pair":2,"n_steps":4,"n_rethink":1,"sigma_cot_value":0.0800,"sigma_max_step":0.5200' \
    '"pass":true'
do
    grep -q -F "$fact" <<<"$OUT_A" \
        || { echo "FAIL: missing '$fact' in cot-distill output" >&2
             echo "$OUT_A"; exit 4; }
done

python3 - "$OUT_A" <<'PY'
import json, sys
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
assert j["tau_rethink"]   == 0.50
assert j["tau_confident"] == 0.20
pairs = j["pairs"]
assert len(pairs) == 2

total_rethink = 0
for p in pairs:
    n    = p["n_steps"]
    nr   = p["n_rethink"]
    sigs = p["sigma_per_step"]
    rats = p["rethink_at"]
    assert n > 0 and n == len(sigs), p
    assert 0 <= nr <= n, p
    assert 0.0 <= p["sigma_cot_value"] <= 1.0, p
    assert 0.0 <= p["sigma_max_step"]  <= 1.0, p
    assert p["sigma_max_step"] == max(sigs), p
    for i in rats:
        assert 1 <= i <= n, f"rethink_at {i} out of range 1..{n}"
        assert sigs[i - 1] >= j["tau_rethink"] - 1e-6, \
            f"rethink at {i} but σ = {sigs[i-1]} < τ"
    for i, s in enumerate(sigs, start=1):
        assert 0.0 <= s <= 1.0
        if s >= j["tau_rethink"] - 1e-6:
            assert i in rats, f"σ={s} ≥ τ but {i} not in rethink_at"
    total_rethink += nr

assert total_rethink == j["rethink_points_total"]
assert j["rethink_points_total"] >= 1, \
    "σ-cot-distill demo must exhibit at least one rethink point"

print("check-sigma-cot-distill: PASS (deterministic, per-step σ parsed, "
      "{} rethink points across {} pairs, mean σ_cot_value = {:.4f})".format(
          j["rethink_points_total"], len(pairs), j["mean_cot_value"]))
PY
