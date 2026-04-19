#!/usr/bin/env bash
#
# v284 σ-Multi-Agent — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 adapter rows canonical (langgraph, crewai,
#     autogen, swarm); all enabled; sigma_middleware
#     on every row; all distinct
#   * 4 a2a rows; decision TRUST iff sigma_message
#     <= tau_a2a=0.40 else VERIFY; both branches
#   * 5 consensus rows with
#     weight_i = (1 − σ_i) / Σ(1 − σ_j);
#     weights sum to 1.0 ± 1e-3; exactly 1 is_winner
#     AND that winner is argmin σ AND argmax weight
#   * 4 routing rows canonical (easy LOCAL /1,
#     medium NEGOTIATE /2, hard CONSENSUS /5,
#     critical HITL /0); every mode fires exactly
#     once; n_agents matches mode
#   * sigma_multiagent in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v284_multi_agent"
[ -x "$BIN" ] || { echo "v284: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v284", d
assert d["chain_valid"] is True, d
assert abs(d["tau_a2a"]    - 0.40) < 1e-6, d
assert abs(d["tau_easy"]   - 0.20) < 1e-6, d
assert abs(d["tau_medium"] - 0.50) < 1e-6, d
assert abs(d["tau_hard"]   - 0.80) < 1e-6, d

A = d["adapter"]
assert len(A) == 4
assert [a["name"] for a in A] == ["langgraph","crewai","autogen","swarm"], A
for a in A:
    assert a["adapter_enabled"]  is True, a
    assert a["sigma_middleware"] is True, a
assert len({a["name"] for a in A}) == 4, A
assert d["n_adapter_rows_ok"]    == 4, d
assert d["adapter_distinct_ok"]  is True, d

X = d["a2a"]
assert len(X) == 4
nt = nv = 0
for r in X:
    exp = "TRUST" if r["sigma_message"] <= d["tau_a2a"] else "VERIFY"
    assert r["decision"] == exp, r
    if r["decision"] == "TRUST":  nt += 1
    else:                          nv += 1
assert nt >= 1 and nv >= 1, (nt, nv)
assert d["n_a2a_rows_ok"]  == 4, d
assert d["n_a2a_trust"]    == nt, d
assert d["n_a2a_verify"]   == nv, d

C = d["consensus"]
assert len(C) == 5
denom = sum(1.0 - c["sigma_agent"] for c in C)
assert denom > 0, C
for c in C:
    expected = (1.0 - c["sigma_agent"]) / denom
    assert abs(c["weight"] - expected) < 1e-4, (c, expected)
assert abs(sum(c["weight"] for c in C) - 1.0) <= 1e-3, C
winners = [c for c in C if c["is_winner"]]
assert len(winners) == 1, winners
argmin_sigma = min(C, key=lambda c: c["sigma_agent"])
argmax_weight = max(C, key=lambda c: c["weight"])
assert winners[0]["agent_id"] == argmin_sigma["agent_id"], (winners, argmin_sigma)
assert winners[0]["agent_id"] == argmax_weight["agent_id"], (winners, argmax_weight)
assert d["n_consensus_rows_ok"]    == 5, d
assert d["consensus_weights_ok"]   is True, d
assert d["consensus_argmax_ok"]    is True, d
assert d["winner_agent_id"]        == winners[0]["agent_id"], d

R = d["routing"]
want_rows = [
    ("easy",     "LOCAL",     1),
    ("medium",   "NEGOTIATE", 2),
    ("hard",     "CONSENSUS", 5),
    ("critical", "HITL",      0),
]
for (lbl, mode, n), r in zip(want_rows, R):
    assert r["label"]    == lbl,  r
    assert r["mode"]     == mode, r
    assert r["n_agents"] == n,    r
assert d["n_routing_rows_ok"]   == 4, d
assert d["routing_distinct_ok"] is True, d
assert d["n_mode_local"]     == 1, d
assert d["n_mode_negotiate"] == 1, d
assert d["n_mode_consensus"] == 1, d
assert d["n_mode_hitl"]      == 1, d

assert 0.0 <= d["sigma_multiagent"] <= 1.0, d
assert d["sigma_multiagent"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v284: non-deterministic" >&2; exit 1; }
