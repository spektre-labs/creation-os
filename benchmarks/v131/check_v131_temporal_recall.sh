#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Merge-gate for v131 σ-Temporal.
#
# Validates:
#   1. self-test green (window + trend + decay + spike + deadline)
#   2. trend-demo: math slope is negative (learning)
#   3. decay weights: older+uncertain < older+confident
#   4. deadline-σ prediction: σ grows with time-fraction-used
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BIN="$ROOT/creation_os_v131_temporal"
if [ ! -x "$BIN" ]; then
    echo "check-v131: building creation_os_v131_temporal..." >&2
    make -s creation_os_v131_temporal
fi

echo "check-v131-temporal-recall: --self-test"
"$BIN" --self-test >/dev/null

echo "check-v131-temporal-recall: trend-demo on decreasing math σ"
TREND_OUT="$("$BIN" --trend-demo)"
echo "  $TREND_OUT"
python3 - <<EOF
import json, re
lines = [l for l in """$TREND_OUT""".splitlines() if l.strip()]
a = json.loads(re.search(r'\{.*\}', lines[0]).group(0))
b = json.loads(re.search(r'\{.*\}', lines[1]).group(0))
assert a["topic"] == "math", a
assert a["slope_per_sec"] < 0, f"math slope must be negative, got {a['slope_per_sec']}"
assert a["n_used"] >= 5, a
assert b["recall_window_7d_events"] >= 5, b
print(f"  math slope_per_day={a['slope_per_day']:.6f}, n_used={a['n_used']}")
EOF

echo "check-v131-temporal-recall: decay weights"
# 30-day old, high σ should decay much more than 30-day old low σ
D_LO="$("$BIN" --decay 2592000 0.10 604800)"   # 30d, σ=0.10, hl=7d
D_HI="$("$BIN" --decay 2592000 0.90 604800)"   # 30d, σ=0.90, hl=7d
echo "  low σ  (30d): $D_LO"
echo "  high σ (30d): $D_HI"
python3 - <<EOF
import json, re
lo = json.loads(re.search(r'\{.*\}', """$D_LO""").group(0))
hi = json.loads(re.search(r'\{.*\}', """$D_HI""").group(0))
assert lo["weight"] > hi["weight"], f"low-σ weight {lo['weight']} must exceed high-σ {hi['weight']}"
assert hi["weight"] < 0.01, f"high-σ 30-day weight must be <0.01, got {hi['weight']}"
print(f"  low-σ={lo['weight']:.6f} > high-σ={hi['weight']:.6f} ok")
EOF

echo "check-v131-temporal-recall: deadline σ rises with pressure"
P0="$("$BIN" --deadline 0.20 0.60 0.05)"
P1="$("$BIN" --deadline 0.20 0.60 0.95)"
echo "  early: $P0"
echo "  late : $P1"
python3 - <<EOF
import json, re
a = json.loads(re.search(r'\{.*\}', """$P0""").group(0))
b = json.loads(re.search(r'\{.*\}', """$P1""").group(0))
assert b["sigma_pred"] > a["sigma_pred"] + 0.3, f"{b['sigma_pred']} vs {a['sigma_pred']}"
print(f"  σ: {a['sigma_pred']:.4f} → {b['sigma_pred']:.4f} ok")
EOF

echo "check-v131-temporal-recall: OK"
