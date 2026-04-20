#!/usr/bin/env bash
#
# v303 σ-Governance — merge-gate check.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v303_governance"
[ -x "$BIN" ] || { echo "v303: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v303_governance", d
assert d["chain_valid"] is True, d
assert abs(d["tau_comm"] - 0.50) < 1e-6, d
assert abs(d["k_crit"] - 0.127) < 1e-6, d
assert abs(d["k_warn"] - 0.20) < 1e-6, d

D = d["decisions"]
want = ["strategy_matches_ops",
        "strategy_partially_realised",
        "strategy_ignored"]
assert [r["label"] for r in D] == want, D
prev = -1.0
for r in D:
    assert r["sigma_decision"] > prev, D
    prev = r["sigma_decision"]
verdicts = [r["verdict"] for r in D]
assert len(set(verdicts)) == 3, D
assert d["dec_rows_ok"]     == 3,    d
assert d["dec_order_ok"]    is True, d
assert d["dec_distinct_ok"] is True, d

M = d["meetings"]
want_m = ["meeting_perfect", "meeting_quarter", "meeting_noise"]
assert [r["label"] for r in M] == want_m, M
prev = -1.0
for r in M:
    expected = 1.0 - r["decisions_realised"] / r["decisions_made"]
    assert abs(r["sigma_meeting"] - expected) < 1e-3, r
    assert r["sigma_meeting"] > prev, M
    prev = r["sigma_meeting"]
assert d["mtg_rows_ok"]     == 3,    d
assert d["mtg_formula_ok"]  is True, d
assert d["mtg_order_ok"]    is True, d

C = d["comms"]
want_c = ["clear_message", "slightly_vague", "highly_vague"]
assert [r["channel"] for r in C] == want_c, C
TAU = d["tau_comm"]
prev = -1.0
for r in C:
    assert r["clear"] is (r["sigma_comm"] <= TAU), r
    assert r["sigma_comm"] > prev, C
    prev = r["sigma_comm"]
n_c = sum(1 for r in C if r["clear"])
n_a = sum(1 for r in C if not r["clear"])
assert n_c >= 1 and n_a >= 1, C
assert d["comm_rows_ok"]     == 3,    d
assert d["comm_order_ok"]    is True, d
assert d["comm_polarity_ok"] is True, d

K = d["institutional_k"]
want_k = ["healthy_org", "warning_org", "collapsing_org"]
assert [r["org"] for r in K] == want_k, K
KC = d["k_crit"]; KW = d["k_warn"]
statuses = []
for r in K:
    expected = r["rho"] * r["i_phi"] * r["f"]
    assert abs(r["k"] - expected) < 1e-3, r
    if r["k"] >= KW:
        exp_status = "VIABLE"
    elif r["k"] >= KC:
        exp_status = "WARNING"
    else:
        exp_status = "COLLAPSE"
    assert r["status"] == exp_status, r
    statuses.append(r["status"])
assert set(statuses) == {"VIABLE", "WARNING", "COLLAPSE"}, K
assert d["kt_rows_ok"]     == 3,    d
assert d["kt_formula_ok"]  is True, d
assert d["kt_polarity_ok"] is True, d

assert 0.0 <= d["sigma_gov"] <= 1.0, d
assert d["sigma_gov"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v303: non-deterministic" >&2; exit 1; }
