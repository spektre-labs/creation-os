#!/usr/bin/env bash
#
# v280 σ-MoE — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 routing rows; decision TOP_K iff
#     σ_routing <= τ_route=0.35 else DIVERSIFY;
#     both branches
#   * 3 signature rows canonical (code, math,
#     creative); familiar KNOWN iff routing_entropy
#     <= τ_sig=0.40 else NOVEL; both branches
#   * 3 prefetch rows canonical (low, mid, high);
#     strategy cascade AGGRESSIVE <=0.20 / BALANCED
#     <=0.50 / CONSERVATIVE else; each exactly once
#   * 3 MoBiE rows canonical (expert_0..expert_2);
#     bits cascade BIT1 <=0.20 / BIT2 <=0.50 /
#     BIT4 else; each exactly once
#   * sigma_moe in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v280_moe"
[ -x "$BIN" ] || { echo "v280: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v280", d
assert d["chain_valid"] is True, d
assert abs(d["tau_route"]        - 0.35) < 1e-6, d
assert abs(d["tau_sig"]          - 0.40) < 1e-6, d
assert abs(d["tau_prefetch_low"] - 0.20) < 1e-6, d
assert abs(d["tau_prefetch_mid"] - 0.50) < 1e-6, d
assert abs(d["tau_shift_low"]    - 0.20) < 1e-6, d
assert abs(d["tau_shift_mid"]    - 0.50) < 1e-6, d

R = d["route"]
assert len(R) == 4
nt = nd = 0
for r in R:
    exp = "TOP_K" if r["sigma_routing"] <= d["tau_route"] else "DIVERSIFY"
    assert r["decision"] == exp, r
    if r["decision"] == "TOP_K": nt += 1
    else:                          nd += 1
assert nt >= 1 and nd >= 1, (nt, nd)
assert d["n_route_rows_ok"]    == 4, d
assert d["n_route_topk"]       == nt, d
assert d["n_route_diversify"]  == nd, d

S = d["sig"]
assert len(S) == 3
assert [x["task"] for x in S] == ["code", "math", "creative"], S
nk = nn_ = 0
for g in S:
    exp = "KNOWN" if g["routing_entropy"] <= d["tau_sig"] else "NOVEL"
    assert g["familiar"] == exp, g
    if g["familiar"] == "KNOWN": nk += 1
    else:                          nn_ += 1
assert nk >= 1 and nn_ >= 1, (nk, nn_)
assert d["n_sig_rows_ok"] == 3, d
assert d["n_sig_known"]   == nk, d
assert d["n_sig_novel"]   == nn_, d

PF = d["prefetch"]
assert len(PF) == 3
assert [x["label"] for x in PF] == ["low", "mid", "high"], PF
na = nb = nc = 0
for p in PF:
    sp = p["sigma_prefetch"]
    if   sp <= d["tau_prefetch_low"]: exp = "AGGRESSIVE"
    elif sp <= d["tau_prefetch_mid"]: exp = "BALANCED"
    else:                              exp = "CONSERVATIVE"
    assert p["strategy"] == exp, p
    if p["strategy"] == "AGGRESSIVE":   na += 1
    if p["strategy"] == "BALANCED":     nb += 1
    if p["strategy"] == "CONSERVATIVE": nc += 1
assert (na, nb, nc) == (1, 1, 1), (na, nb, nc)
assert d["n_prefetch_rows_ok"] == 3, d
assert d["n_prefetch_agg"]  == 1, d
assert d["n_prefetch_bal"]  == 1, d
assert d["n_prefetch_cons"] == 1, d

M = d["mobie"]
assert len(M) == 3
assert [x["name"] for x in M] == ["expert_0", "expert_1", "expert_2"], M
n1 = n2 = n4 = 0
for m in M:
    ss = m["sigma_shift"]
    if   ss <= d["tau_shift_low"]: exp = "BIT1"
    elif ss <= d["tau_shift_mid"]: exp = "BIT2"
    else:                           exp = "BIT4"
    assert m["bits"] == exp, m
    if m["bits"] == "BIT1": n1 += 1
    if m["bits"] == "BIT2": n2 += 1
    if m["bits"] == "BIT4": n4 += 1
assert (n1, n2, n4) == (1, 1, 1), (n1, n2, n4)
assert d["n_mobie_rows_ok"] == 3, d
assert d["n_mobie_bit1"] == 1, d
assert d["n_mobie_bit2"] == 1, d
assert d["n_mobie_bit4"] == 1, d

assert 0.0 <= d["sigma_moe"] <= 1.0, d
assert d["sigma_moe"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v280: non-deterministic" >&2; exit 1; }
