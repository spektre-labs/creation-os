#!/usr/bin/env bash
#
# v256 σ-Wellness — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * session: duration_hours >= 0, n_requests >= 0,
#     signal_trend in {STABLE, DEGRADING}, session_ok
#   * 3 nudge fixtures exercised in order:
#       [0] FIRE (first past tau=4.0, cfg=true,
#             already_fired_before=false)
#       [1] DENY (already_fired_before=true → teeth)
#       [2] OPT_OUT (config_enabled=false)
#     exactly 1 FIRE, 1 DENY, 1 OPT_OUT
#   * 3 boundary signals watched; boundary reminder
#     fires AT MOST ONCE per session
#   * 3 cognitive-load rows in canonical order:
#       LOW→NONE, MED→SUMMARISE, HIGH→SIMPLIFY
#     n_load_rows_ok == 3
#   * sigma_wellness in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v256_wellness"
[ -x "$BIN" ] || { echo "v256: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v256", d
assert d["chain_valid"] is True, d
assert abs(d["tau_duration_hours"] - 4.0) < 1e-6, d

s = d["session"]
assert s["duration_hours"] >= 0.0, s
assert s["n_requests"]     >= 0,   s
assert s["signal_trend"] in {"STABLE","DEGRADING"}, s
assert s["session_ok"] is True, s

assert d["n_fire"]    == 1, d
assert d["n_deny"]    == 1, d
assert d["n_opt_out"] == 1, d
nudge = d["nudge"]
assert nudge[0]["decision"] == "FIRE",    nudge
assert nudge[1]["decision"] == "DENY",    nudge
assert nudge[2]["decision"] == "OPT_OUT", nudge
assert nudge[1]["already_fired_before"] is True, nudge

boundary = d["boundary"]
assert len(boundary) == 3, boundary
for b in boundary:
    assert b["watched"] is True, b
reminders = sum(1 for b in boundary if b["reminder_fired"])
assert reminders <= 1, reminders
assert d["n_boundary_reminders"] == reminders, d

want_load = [
    ("LOW",  "NONE"),
    ("MED",  "SUMMARISE"),
    ("HIGH", "SIMPLIFY"),
]
got_load = [(r["level"], r["action"]) for r in d["load"]]
assert got_load == want_load, got_load
assert d["n_load_rows_ok"] == 3, d

assert 0.0 <= d["sigma_wellness"] <= 1.0, d
assert d["sigma_wellness"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v256: non-deterministic" >&2; exit 1; }
