#!/usr/bin/env bash
#
# v268 σ-Continuous-Batch — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 queue requests; priority_slot permutation of
#     [1..6] AND matches argsort(+sigma_difficulty)
#     (low σ first, fast path); queue_order_ok
#   * 2 preemption scenarios; preempted matches
#     (sigma_urgency_arrival > sigma_urgency_incumbent);
#     >=1 true AND >=1 false
#   * 3 load levels canonical (low, medium, high);
#     sigma_load strictly ascending AND batch_size
#     strictly ascending; batch_monotone_ok
#   * 2 cost scenarios; total_local_eur < total_api_eur
#   * sigma_continuous_batch in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v268_continuous_batch"
[ -x "$BIN" ] || { echo "v268: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v268", d
assert d["chain_valid"] is True, d

Q = d["queue"]
assert len(Q) == 6
slots = sorted(q["priority_slot"] for q in Q)
assert slots == [1,2,3,4,5,6], Q
by_slot = sorted(Q, key=lambda q: q["priority_slot"])
for i in range(1, 6):
    assert by_slot[i-1]["sigma_difficulty"] <= by_slot[i]["sigma_difficulty"], by_slot
for q in Q:
    assert 0.0 <= q["sigma_difficulty"] <= 1.0, q
    assert q["engine"], q
assert d["n_queue_ok"] == 6, d
assert d["queue_order_ok"] is True, d

P = d["preempt"]
assert len(P) == 2
n_t = n_f = 0
for p in P:
    exp = p["sigma_urgency_arrival"] > p["sigma_urgency_incumbent"]
    assert p["preempted"] == exp, p
    assert p["winner"], p
    if p["preempted"]: n_t += 1
    else:              n_f += 1
assert n_t >= 1 and n_f >= 1, (n_t, n_f)
assert d["n_preempt_ok"] == 2, d
assert d["n_preempt_true"] == n_t, d
assert d["n_preempt_false"] == n_f, d

L = d["levels"]
assert [l["level"] for l in L] == ["low","medium","high"], L
for i in range(1, 3):
    assert L[i]["sigma_load"] > L[i-1]["sigma_load"], L
    assert L[i]["batch_size"] > L[i-1]["batch_size"], L
assert d["n_levels_ok"] == 3, d
assert d["batch_monotone_ok"] is True, d

C = d["costs"]
assert len(C) == 2
assert d["total_local_eur"] < d["total_api_eur"], d
assert d["n_costs_ok"] == 2, d

assert 0.0 <= d["sigma_continuous_batch"] <= 1.0, d
assert d["sigma_continuous_batch"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v268: non-deterministic" >&2; exit 1; }
