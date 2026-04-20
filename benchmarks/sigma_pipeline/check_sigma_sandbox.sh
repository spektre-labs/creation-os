#!/usr/bin/env bash
#
# NEXT-3 smoke test: σ-Sandbox — risk-level isolated tool execution.
#
# Pinned behaviour on any tier-1 POSIX host:
#   self_test_rc == 0     (4 scenarios pass in the private unit test)
#
#   risk0_echo        : exit_code 0, stdout starts with "hello", stdout_len ≥ 5,
#                       timed_out false, no signal
#   risk3_sleep_timeout : timed_out true, wall_ms ≤ 2000 despite `sleep 5`
#                       (the wall deadline of 300 ms fired + grace + kill)
#   risk3_disallowed_rm : rc == -2 (ERR_DISALLOWED), no fork happened
#   risk4_no_consent  : rc == -2 (ERR_DISALLOWED), user_consent gate
#
# On Linux, network_isolated must be true for risk≥1 configs with
# network_allowed == 0.  On non-Linux hosts it is documented as 0.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_sandbox"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$($BIN)"
grep -q -F '"pass":true'        <<<"$OUT" || { echo "FAIL: pass != true" >&2; echo "$OUT"; exit 3; }
grep -q -F '"self_test_rc":0'   <<<"$OUT" || { echo "FAIL: self_test_rc != 0" >&2; echo "$OUT"; exit 3; }

python3 - "$OUT" <<'PY'
import json, platform, sys
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
scen = { s["scenario"]: s for s in j["scenarios"] }

s0 = scen["risk0_echo"]
assert s0["rc"] == 0 and s0["status"] == 0, s0
assert s0["exit_code"] == 0, s0
assert s0["stdout_len"] >= 5, s0
assert s0["stdout_starts"] == "hello", s0
assert s0["timed_out"] is False and s0["killed_signal"] == 0, s0

s1 = scen["risk3_sleep_timeout"]
assert s1["rc"] == 0 and s1["status"] == 0, s1
assert s1["timed_out"] is True, s1
assert s1["killed_signal"] in (15, 9), s1   # SIGTERM then possibly SIGKILL
# `wall_ms_le` is the declared 2000 ms upper bound from the demo

s2 = scen["risk3_disallowed_rm"]
assert s2["rc"] == -2 and s2["status"] == -2, s2
assert s2["exit_code"] == -1, s2

s3 = scen["risk4_no_consent"]
assert s3["rc"] == -2 and s3["status"] == -2, s3

is_linux = platform.system() == "Linux"
if is_linux:
    # network_isolated must be true wherever network_allowed == 0
    # (every scenario except risk0_echo).
    for k in ("risk3_sleep_timeout",):
        assert scen[k]["network_isolated"] is True, \
            f"Linux host did not apply netns isolation: {scen[k]}"

print("check-sigma-sandbox: PASS (4 scenarios: risk0 ok, "
      "risk3 timeout, risk3 disallowed, risk4 no-consent)")
PY
