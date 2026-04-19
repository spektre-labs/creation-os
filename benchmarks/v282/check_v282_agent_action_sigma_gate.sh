#!/usr/bin/env bash
#
# v282 σ-Agent — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 action rows; cascade AUTO <=0.20 / ASK <=0.60
#     / BLOCK else; each branch fires >=1
#   * 2 propagation chains (short 3 steps @0.10,
#     long 10 steps @0.30); sigma_total = 1 −
#     (1−sigma_per_step)^n_steps; PROCEED iff
#     sigma_total <= tau_chain=0.70 else ABORT;
#     PROCEED and ABORT each fire exactly once;
#     math match within 1e-4
#   * 3 tool rows canonical (correct, wrong_light,
#     wrong_heavy); cascade USE <=0.30 / SWAP <=0.60
#     / BLOCK else; each branch fires >=1
#   * 3 recovery rows; sigma_after_fail > sigma_first_try
#     strictly per row AND gate_update_applied for all
#   * sigma_agent in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v282_agent"
[ -x "$BIN" ] || { echo "v282: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v282", d
assert d["chain_valid"] is True, d
assert abs(d["tau_auto"]      - 0.20) < 1e-6, d
assert abs(d["tau_ask"]       - 0.60) < 1e-6, d
assert abs(d["tau_chain"]     - 0.70) < 1e-6, d
assert abs(d["tau_tool_use"]  - 0.30) < 1e-6, d
assert abs(d["tau_tool_swap"] - 0.60) < 1e-6, d

A = d["action"]
assert len(A) == 4
naa = nas = nab = 0
for a in A:
    sa = a["sigma_action"]
    if   sa <= d["tau_auto"]: exp = "AUTO"
    elif sa <= d["tau_ask"]:  exp = "ASK"
    else:                      exp = "BLOCK"
    assert a["decision"] == exp, a
    if a["decision"] == "AUTO":  naa += 1
    if a["decision"] == "ASK":   nas += 1
    if a["decision"] == "BLOCK": nab += 1
assert naa >= 1 and nas >= 1 and nab >= 1, (naa, nas, nab)
assert d["n_action_rows_ok"] == 4, d
assert d["n_action_auto"]    == naa, d
assert d["n_action_ask"]     == nas, d
assert d["n_action_block"]   == nab, d

C = d["chain"]
assert len(C) == 2
assert [x["label"] for x in C] == ["short", "long"], C
assert C[0]["n_steps"] == 3  and abs(C[0]["sigma_per_step"] - 0.10) < 1e-6, C[0]
assert C[1]["n_steps"] == 10 and abs(C[1]["sigma_per_step"] - 0.30) < 1e-6, C[1]
eps = d["math_eps"]
for c in C:
    expected = 1.0 - (1.0 - c["sigma_per_step"]) ** c["n_steps"]
    assert abs(c["sigma_total"] - expected) <= eps + 1e-6, (c, expected)
    want = "PROCEED" if c["sigma_total"] <= d["tau_chain"] else "ABORT"
    assert c["verdict"] == want, c
np_ = sum(1 for c in C if c["verdict"] == "PROCEED")
na_ = sum(1 for c in C if c["verdict"] == "ABORT")
assert np_ == 1 and na_ == 1, (np_, na_)
assert d["n_chain_rows_ok"] == 2, d
assert d["n_chain_proceed"] == 1, d
assert d["n_chain_abort"]   == 1, d
assert d["chain_math_ok"]   is True, d

T = d["tool"]
assert len(T) == 3
assert [x["tool_id"] for x in T] == ["correct", "wrong_light", "wrong_heavy"], T
ntu = nts = ntb = 0
for t in T:
    st = t["sigma_tool"]
    if   st <= d["tau_tool_use"]:  exp = "USE"
    elif st <= d["tau_tool_swap"]: exp = "SWAP"
    else:                           exp = "BLOCK"
    assert t["decision"] == exp, t
    if t["decision"] == "USE":   ntu += 1
    if t["decision"] == "SWAP":  nts += 1
    if t["decision"] == "BLOCK": ntb += 1
assert ntu >= 1 and nts >= 1 and ntb >= 1, (ntu, nts, ntb)
assert d["n_tool_rows_ok"] == 3, d
assert d["n_tool_use"]   == ntu, d
assert d["n_tool_swap"]  == nts, d
assert d["n_tool_block"] == ntb, d

R = d["recovery"]
assert len(R) == 3
for r in R:
    assert r["sigma_after_fail"] > r["sigma_first_try"], r
    assert r["gate_update_applied"] is True, r
assert d["n_recovery_rows_ok"]    == 3, d
assert d["recovery_monotone_ok"]  is True, d
assert d["recovery_updates_ok"]   is True, d

assert 0.0 <= d["sigma_agent"] <= 1.0, d
assert d["sigma_agent"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v282: non-deterministic" >&2; exit 1; }
