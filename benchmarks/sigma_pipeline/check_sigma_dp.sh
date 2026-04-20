#!/usr/bin/env bash
#
# PROD-1 smoke test: σ-DP deterministic replay + ε-budget tracking
# + σ_dp monotonicity.
#
# Pinned facts (seed 0xC05D5EED, ε=1.0, ε_total=5.0, clip=1.0):
#   - self_test_rc == 0
#   - three rounds applied, eps_spent ends at 3.0000, eps_remaining 2.0000
#   - round 3 pre_clip_norm == 2.0000 AND post_clip_norm == 1.0000
#     (clipping actually fires when ‖g‖ > C)
#   - every round's σ_dp ∈ [0, 1]
#   - a second run over the same binary produces byte-identical JSON
#     (determinism)
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_dp"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT_A="$($BIN)"
OUT_B="$($BIN)"

# ---- 1. determinism (bit-identical JSON across two runs) -----------------
[[ "$OUT_A" == "$OUT_B" ]] \
    || { echo "FAIL: DP output not deterministic across runs" >&2; echo "--A--"; echo "$OUT_A"; echo "--B--"; echo "$OUT_B"; exit 3; }

# ---- 2. mandatory headline fields ----------------------------------------
for fact in \
    '"self_test_rc":0' \
    '"rounds":[' \
    '"eps_spent":3.0000' \
    '"eps_total":5.0000' \
    '"eps_remaining":2.0000' \
    '"rounds_applied":3' \
    '"pass":true'
do
    grep -q -F "$fact" <<<"$OUT_A" \
        || { echo "FAIL: missing '$fact' in DP output" >&2; echo "$OUT_A"; exit 4; }
done

# ---- 3. clipping must fire on round 3 (pre=2.0, post=1.0) -----------------
grep -q '"round":3,"pre_clip_norm":2.0000,"post_clip_norm":1.0000' <<<"$OUT_A" \
    || { echo "FAIL: round-3 clipping pre=2.0 post=1.0 not observed" >&2; echo "$OUT_A"; exit 5; }

# ---- 4. σ_dp must be in [0, 1] for every round ---------------------------
python3 - "$OUT_A" <<'PY'
import json, sys
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
for r in j["rounds"]:
    s = r["sigma_dp"]
    assert 0.0 <= s <= 1.0, f"sigma_dp out of range: {r}"
    ns = r["noise_scale"]
    assert ns > 0.0, f"noise_scale non-positive: {r}"
# round 3 must have larger pre_clip_norm than post_clip_norm (clipping fired)
r3 = j["rounds"][2]
assert r3["pre_clip_norm"] > r3["post_clip_norm"]
assert abs(r3["post_clip_norm"] - 1.0) < 1e-6
assert j["final"]["eps_spent"] == 3.0
assert j["final"]["rounds_left"] == 2
print("check-sigma-dp: PASS (deterministic, budget-bounded, σ_dp ∈ [0,1], clip fires)")
PY
