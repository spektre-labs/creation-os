#!/usr/bin/env bash
#
# HERMES-5 smoke test: σ-watchdog quality monitoring.
#
# Pinned facts (per watchdog_main.c two-scenario script):
#   self_test == 0
#   tau_sigma_up = 0.01, tau_escal_up = 0.005
#   scenarios: ["drifting", "steady"]
#   drifting:
#     count=168, alert_sigma=true, alert_escal=true, health=ALERT
#     sigma_trend_per_day > tau_sigma_up
#     escal_trend_per_day > tau_escal_up
#     plan.remedy == "notify_human"
#   steady:
#     count=168, alert_sigma=false, alert_escal=false, health=HEALTHY
#     sigma_trend_per_day == 0, escal_trend_per_day == 0
#     plan.remedy == "none"
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_watchdog"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_watchdog: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_watchdog", doc
assert doc["self_test"] == 0, doc
assert abs(doc["tau_sigma_up"] - 0.01)  < 1e-4, doc
assert abs(doc["tau_escal_up"] - 0.005) < 1e-4, doc

scen = doc["scenarios"]
assert [s["label"] for s in scen] == ["drifting", "steady"], scen

drift = scen[0]
assert drift["count"] == 168, drift
assert drift["alert_sigma"] is True, drift
assert drift["alert_escal"] is True, drift
assert drift["health"] == "ALERT", drift
assert drift["sigma_trend_per_day"] > doc["tau_sigma_up"], drift
assert drift["escal_trend_per_day"] > doc["tau_escal_up"], drift
# σ_1h should already be higher than σ_7d for a positive drift.
assert drift["sigma_mean_1h"] > drift["sigma_mean_7d"], drift
assert drift["plan"]["remedy"] == "notify_human", drift

steady = scen[1]
assert steady["count"] == 168, steady
assert steady["alert_sigma"] is False, steady
assert steady["alert_escal"] is False, steady
assert steady["health"] == "HEALTHY", steady
assert abs(steady["sigma_trend_per_day"]) < 1e-3, steady
assert abs(steady["escal_trend_per_day"]) < 1e-3, steady
assert steady["plan"]["remedy"] == "none", steady

print("check_sigma_watchdog: PASS", {
    "drifting_sigma_trend": drift["sigma_trend_per_day"],
    "drifting_remedy":      drift["plan"]["remedy"],
    "steady_health":        steady["health"],
})
PY
