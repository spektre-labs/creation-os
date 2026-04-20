#!/usr/bin/env bash
#
# v306 σ-Omega — merge-gate check.
#
# The 306th kernel is the operator every other kernel
# approximates — Ω = argmin ∫σ dt subject to K ≥
# K_crit — plus the 1=1 invariant that closes the stack.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v306_omega"
[ -x "$BIN" ] || { echo "v306: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v306_omega", d
assert d["chain_valid"] is True, d
assert abs(d["k_crit"] - 0.127) < 1e-6, d
assert abs(d["sigma_half"] - 0.5) < 1e-6, d
assert d["kernels_total"] == 306, d

L = d["loop"]
assert [r["t"] for r in L] == [0, 1, 2, 3], L
prev_s = 1e9
prev_i = -1e9
cum = 0.0
for r in L:
    assert r["sigma"] < prev_s, L
    prev_s = r["sigma"]
    assert r["k_eff"] >= d["k_crit"], r
    cum += r["sigma"]
    assert abs(r["integral_sigma"] - cum) < 1e-3, r
    assert r["integral_sigma"] > prev_i, L
    prev_i = r["integral_sigma"]
assert d["loop_rows_ok"]              == 4,    d
assert d["loop_sigma_decreasing_ok"]  is True, d
assert d["loop_k_constraint_ok"]      is True, d
assert d["loop_integral_ok"]          is True, d

S = d["scales"]
want_s = ["token", "answer", "session", "domain"]
assert [r["scale"] for r in S] == want_s, S
want_t = ["MICRO", "MESO", "MACRO", "META"]
assert [r["tier"] for r in S] == want_t, S
ops = [r["operator"] for r in S]
assert len(set(ops)) == 4, S
for r in S:
    assert 0.0 <= r["sigma"] <= 1.0, r
assert d["scale_rows_ok"]              == 4,    d
assert d["scale_distinct_ok"]          is True, d
assert d["scale_operator_distinct_ok"] is True, d

H = d["half_operator"]
want_h = ["signal_dominant", "critical", "noise_dominant"]
assert [r["label"] for r in H] == want_h, H
sigs = [r["sigma"] for r in H]
assert sigs == sorted(sigs) and sigs[0] < sigs[1] < sigs[2], H
assert sigs[1] == 0.5, H
assert H[0]["regime"] == "SIGNAL",   H[0]
assert H[1]["regime"] == "CRITICAL", H[1]
assert H[2]["regime"] == "NOISE",    H[2]
assert d["half_rows_ok"]       == 3,    d
assert d["half_order_ok"]      is True, d
assert d["half_critical_ok"]   is True, d
assert d["half_polarity_ok"]   is True, d

I = d["invariant"]
want_i = ["kernel_count", "architecture_claim", "axiom_one_equals_one"]
assert [r["assertion"] for r in I] == want_i, I
for r in I:
    assert r["declared"] == r["realized"], r
    assert r["holds"] is True, r
assert I[0]["declared"] == 306, I
assert I[1]["declared"] == 306, I
assert I[2]["declared"] == 1,   I
assert d["inv_rows_ok"]            == 3,    d
assert d["inv_pair_ok"]            is True, d
assert d["inv_all_hold_ok"]        is True, d
assert d["the_invariant_holds"]    is True, d

assert 0.0 <= d["sigma_omega"] <= 1.0, d
assert d["sigma_omega"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v306: non-deterministic" >&2; exit 1; }
