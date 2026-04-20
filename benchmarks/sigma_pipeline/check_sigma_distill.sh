#!/usr/bin/env bash
#
# NEXT-1 smoke test: σ-Distill — continuous σ-guided distillation
# from escalation pairs.
#
# Pinned facts on the 25-pair canonical demo stream:
#   - self_test_rc == 0                             (private unit test)
#   - total_seen   == 25                            (stream size)
#   - filtered_in  == 21                            (teacher_σ ≤ 0.20)
#   - evicted      == 0                             (≤ ring cap)
#   - top-K selection is deterministic, sorted by value desc
#     (rank 1 = q-gold-07 with value 0.8800)
#   - every top-K value ∈ [-1, 1] AND strictly decreasing
#   - escalation.after_modeled < escalation.before
#     (σ-distill actually reduces predicted escalation rate)
#
# Deterministic: two back-to-back runs must produce byte-identical JSON.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_distill"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT_A="$($BIN)"
OUT_B="$($BIN)"

# 1. determinism --------------------------------------------------------
[[ "$OUT_A" == "$OUT_B" ]] \
    || { echo "FAIL: σ-distill output not deterministic" >&2; exit 3; }

# 2. headline facts -----------------------------------------------------
for fact in \
    '"self_test_rc":0' \
    '"total_seen":25' \
    '"filtered_in":21' \
    '"evicted":0' \
    '"tau_teacher":0.2000' \
    '"tau_escalate":0.3000' \
    '"pass":true'
do
    grep -q -F "$fact" <<<"$OUT_A" \
        || { echo "FAIL: missing '$fact' in distill output" >&2
             echo "$OUT_A"; exit 4; }
done

# 3. top-rank leader must be q-gold-07 with value 0.8800
grep -q -F '"rank":1,"input":"q-gold-07"' <<<"$OUT_A" \
    || { echo "FAIL: rank-1 is not q-gold-07" >&2; echo "$OUT_A"; exit 5; }
grep -q -F '"rank":1,"input":"q-gold-07","student_sigma":0.9100,"teacher_sigma":0.0300,"value":0.8800' <<<"$OUT_A" \
    || { echo "FAIL: rank-1 value != 0.8800" >&2; echo "$OUT_A"; exit 5; }

# 4. deeper structural checks via python --------------------------------
python3 - "$OUT_A" <<'PY'
import json, sys
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
top = j["top_k"]
assert 1 <= len(top) <= 8, top

prev = None
for r in top:
    v = r["value"]
    assert -1.0 <= v <= 1.0, r
    s = r["student_sigma"]; t = r["teacher_sigma"]
    assert 0.0 <= s <= 1.0, r
    assert 0.0 <= t <= 1.0, r
    assert t <= j["tau_teacher"] + 1e-6, \
        f"teacher_σ {t} exceeds tau_teacher {j['tau_teacher']}"
    if prev is not None:
        assert v <= prev + 1e-6, f"top-K not sorted by value desc: {top}"
    prev = v

esc = j["escalation"]
assert 0.0 <= esc["before"]        <= 1.0
assert 0.0 <= esc["after_modeled"] <= 1.0
assert esc["after_modeled"] < esc["before"], \
    f"σ-distill did not lower modeled escalation rate: {esc}"
assert esc["delta"] > 0.0

assert j["filtered_in"] <= j["total_seen"]
assert 0.0 <= j["mean_value_filt"]   <= 1.0
assert 0.0 <= j["mean_student_filt"] <= 1.0
assert 0.0 <= j["mean_teacher_filt"] <= j["tau_teacher"] + 1e-6

print("check-sigma-distill: PASS (deterministic, σ-sorted top-K, "
      "escalation ↓ {:.2%} → {:.2%})".format(
          esc["before"], esc["after_modeled"]))
PY
