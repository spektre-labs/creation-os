#!/usr/bin/env bash
#
# v302 σ-Green — merge-gate check.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v302_green"
[ -x "$BIN" ] || { echo "v302: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v302_green", d
assert d["chain_valid"] is True, d

B = d["budget"]
want = ["easy", "medium", "hard"]
assert [r["difficulty"] for r in B] == want, B
prev_s, prev_e = -1.0, -1.0
for r in B:
    assert r["sigma_difficulty"] > prev_s, B
    assert r["energy_j"] > prev_e, B
    prev_s, prev_e = r["sigma_difficulty"], r["energy_j"]
tiers = [r["model_tier"] for r in B]
assert len(set(tiers)) == 3, B
assert d["budget_rows_ok"]     == 3,    d
assert d["budget_order_ok"]    is True, d
assert d["budget_distinct_ok"] is True, d

S = d["schedule"]
want_s = ["urgent_green", "low_urgency_brown", "urgent_brown"]
assert [r["label"] for r in S] == want_s, S
for r in S:
    expected = (r["urgency"] == "HIGH") or (r["grid"] == "GREEN")
    assert r["processed"] is expected, r
n_p = sum(1 for r in S if r["processed"])
n_d = sum(1 for r in S if not r["processed"])
assert n_p >= 1 and n_d >= 1, S
assert d["sched_rows_ok"]     == 3,    d
assert d["sched_rule_ok"]     is True, d
assert d["sched_polarity_ok"] is True, d

V = d["savings"]
want_v = ["baseline", "sigma_gated_light", "sigma_gated_heavy"]
assert [r["regime"] for r in V] == want_v, V
prev_r, prev_e = -1.0, 1e9
for r in V:
    expected = r["abstained_tokens"] / r["total_tokens"]
    assert abs(r["saved_ratio"] - expected) < 1e-3, r
    assert r["saved_ratio"] >= prev_r, V
    assert r["energy_j"] <= prev_e, V
    prev_r, prev_e = r["saved_ratio"], r["energy_j"]
assert V[0]["saved_ratio"] == 0.0, V[0]
assert V[2]["saved_ratio"] > V[0]["saved_ratio"], V
assert d["save_rows_ok"]              == 3,    d
assert d["save_ratio_ok"]             is True, d
assert d["save_order_ok"]             is True, d
assert d["save_energy_decrease_ok"]   is True, d

J = d["j_per_reliable"]
want_j = ["unfiltered", "soft_filter", "hard_filter"]
assert [r["regime"] for r in J] == want_j, J
prev = 1e9
for r in J:
    expected = r["energy_j"] / r["reliable_tokens"]
    assert abs(r["j_per_reliable"] - expected) < 1e-3, r
    assert r["j_per_reliable"] < prev, J
    prev = r["j_per_reliable"]
assert d["jpt_rows_ok"]       == 3,    d
assert d["jpt_formula_ok"]    is True, d
assert d["jpt_decrease_ok"]   is True, d

assert 0.0 <= d["sigma_green"] <= 1.0, d
assert d["sigma_green"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v302: non-deterministic" >&2; exit 1; }
