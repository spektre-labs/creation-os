#!/usr/bin/env bash
# Smoke test for the cost-measurement driver.
#
# Uses the deterministic built-in demo fixture (10 rows, 3 escalations
# at τ=0.75) so the exact numbers below are pinned by construction.
# Any drift breaks the smoke test — the numbers ARE the contract.
set -euo pipefail

cd "$(dirname "$0")/../.."

PY=".venv-bitnet/bin/python"
if [[ ! -x "$PY" ]]; then
    PY=python3
fi

OUT_JSON="$(mktemp -t cos_cost_summary.XXXXXX.json)"
OUT_MD="$(mktemp -t cos_cost_report.XXXXXX.md)"
trap 'rm -f "$OUT_JSON" "$OUT_MD"' EXIT

PYTHONPATH=scripts "$PY" -m sigma_pipeline.cost_measure \
    --fixture demo --tau-escalate 0.75 \
    --eur-local 0.0005 --eur-api 0.02 \
    --out "$OUT_JSON" --markdown "$OUT_MD" 2>/dev/null

"$PY" - "$OUT_JSON" <<'PYEOF'
import json, sys, math
s = json.load(open(sys.argv[1]))
# Deterministic expected values for the built-in demo fixture.
expected = {
    "n_total":         10,
    "n_escalated":      3,
    "n_local":          7,
    "savings_frac":   0.675,
    "accuracy_api":    1.0,
    "accuracy_local":  0.7,
    "accuracy_hybrid": 1.0,
    "accuracy_retained": 1.0,
    "tau_escalate":   0.75,
}
for k, v in expected.items():
    got = s[k]
    if isinstance(v, float):
        if not math.isclose(got, v, abs_tol=1e-6):
            print(f"FAIL: {k} = {got} != {v}"); sys.exit(5)
    else:
        if got != v:
            print(f"FAIL: {k} = {got} != {v}"); sys.exit(5)
# Cost arithmetic sanity: cost_api_only = 10·0.02 = 0.20
# cost_hybrid = 10·0.0005 + 3·0.02 = 0.065.
assert math.isclose(s["cost_api_only"], 0.20,  abs_tol=1e-6), s
assert math.isclose(s["cost_hybrid"],   0.065, abs_tol=1e-6), s
print(f"  · n={s['n_total']} esc={s['n_escalated']} "
      f"savings={s['savings_frac']:.1%} "
      f"acc_hybrid={s['accuracy_hybrid']:.1%} "
      f"acc_retained={s['accuracy_retained']:.1%}")
PYEOF

grep -q "Creation OS saves" "$OUT_MD" \
    || { echo "FAIL: headline line missing" >&2; exit 6; }
grep -q "68%"               "$OUT_MD" \
    || { echo "FAIL: 68% savings headline missing" >&2; exit 7; }
grep -q "100%"              "$OUT_MD" \
    || { echo "FAIL: 100% accuracy-retained missing" >&2; exit 8; }

echo "check-cost-measure: PASS (pinned numbers: 3/10 escalate, 67.5% savings, "
echo "                     100% accuracy retained, 1:1 parity with speculative.cost_savings)"
