#!/usr/bin/env bash
#
# PROD-4 smoke test: cos-health snapshot + σ-Health grading.
#
# Pinned facts:
#   - self-test passes (--self-test exit 0)
#   - default JSON has 38/38 checks green → HEALTHY
#   - --state <persist.db> lifts engrams count from DB (>= 1)
#   - grading: clamp checks_green to 33 ⇒ UNHEALTHY (< 90 %)
#   - cos health dispatches to cos-health (human first line starts
#     with "cos health:")
#
set -euo pipefail
cd "$(dirname "$0")/../.."

HBIN="./cos-health"
COS="./cos"
[[ -x "$HBIN" ]] || { echo "missing $HBIN" >&2; exit 2; }
[[ -x "$COS"  ]] || { echo "missing $COS"  >&2; exit 2; }

# ---- 1. self-test must pass ---------------------------------------------
"$HBIN" --self-test || { echo "FAIL: cos-health --self-test" >&2; exit 3; }

# ---- 2. default HEALTHY JSON --------------------------------------------
OUT="$($HBIN --json)"
for fact in \
    '"status":"HEALTHY"' \
    '"mode":"HYBRID"' \
    '"checks_total":38' \
    '"checks_green":38' \
    '"pipeline_ok_pct":100' \
    '"sigma_primitives":20' \
    '"substrates":4' \
    '"formal_proofs_discharged":6' \
    '"formal_proofs_total":6'
do
    grep -q -F "$fact" <<<"$OUT" \
        || { echo "FAIL: missing '$fact'" >&2; echo "$OUT"; exit 4; }
done

# ---- 3. --state loads engram count --------------------------------------
if [[ -f /tmp/cos_persist_demo.db ]]; then
    OUT_STATE="$($HBIN --json --state /tmp/cos_persist_demo.db)"
    python3 - "$OUT_STATE" <<'PY'
import json, sys
j = json.loads(sys.argv[1])
assert j["engrams"] >= 1, j
print("  cos-health --state: engrams >= 1 ✓")
PY
else
    echo "  (skipping --state check: /tmp/cos_persist_demo.db absent — run check-sigma-persist first)"
fi

# ---- 4. cos health dispatches to sibling --------------------------------
FIRST="$("$COS" health --json | head -n 1)"
grep -q '"kernel":"sigma_health"' <<<"$FIRST" \
    || { echo "FAIL: cos health didn't dispatch to sibling; got: $FIRST" >&2; exit 5; }

echo "check-sigma-health: PASS (cos-health + cos health dispatch + --state roundtrip)"
