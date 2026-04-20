#!/usr/bin/env bash
#
# v296 σ-Antifragile — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 stress cycles (cycle_1/cycle_2/cycle_3);
#     stress_level strictly monotonically increasing
#     AND sigma_after strictly monotonically
#     DECREASING — antifragile, not merely robust
#   * 3 volatility regimes (unstable/stable/
#     antifragile); 3 DISTINCT classifications;
#     ANTIFRAGILE iff σ_std > 0.03 AND trend=DECREASING;
#     STABLE iff σ_std ≤ 0.03; UNSTABLE iff
#     σ_std > 0.15 AND trend=NONE
#   * 3 vaccine rows (dose_small/dose_medium/
#     real_attack); noise strictly increasing; all
#     survived=true; exactly 2 vaccines AND exactly
#     1 real attack survived because_trained=true
#   * 3 barbell rows (safe_mode/experimental_mode/
#     middle_compromise); safe+exp=1.0; middle=0.0;
#     τ_safe < τ_exp; safe and experimental kept AND
#     middle rejected
#   * sigma_antifragile in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v296_antifragile"
[ -x "$BIN" ] || { echo "v296: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v296", d
assert d["chain_valid"] is True, d

ST = d["stress"]
want_s = ["cycle_1","cycle_2","cycle_3"]
assert [r["label"] for r in ST] == want_s, ST
prev_stress, prev_sigma = -1.0, 2.0
for r in ST:
    assert r["stress_level"] > prev_stress, r
    assert r["sigma_after"]  < prev_sigma,  r
    prev_stress, prev_sigma = r["stress_level"], r["sigma_after"]
assert d["n_stress_rows_ok"]   == 3, d
assert d["stress_monotone_ok"] is True, d

VO = d["vol"]
want_v = ["unstable","stable","antifragile"]
assert [r["regime"] for r in VO] == want_v, VO
classes = set()
for r in VO:
    classes.add(r["classification"])
    if r["classification"] == "ANTIFRAGILE":
        assert r["sigma_std"] > 0.03 - 1e-6, r
        assert r["trend"] == "DECREASING", r
    elif r["classification"] == "STABLE":
        assert r["sigma_std"] <= 0.03 + 1e-6, r
    elif r["classification"] == "UNSTABLE":
        assert r["sigma_std"] > 0.15 - 1e-6, r
        assert r["trend"] == "NONE", r
assert classes == {"UNSTABLE","STABLE","ANTIFRAGILE"}, classes
assert d["n_vol_rows_ok"]     == 3, d
assert d["vol_classify_ok"]   is True, d

VA = d["vac"]
want_c = ["dose_small","dose_medium","real_attack"]
assert [r["label"] for r in VA] == want_c, VA
prev = -1.0
n_vac = 0
n_real_trained = 0
for r in VA:
    assert r["noise"]    > prev, r
    prev = r["noise"]
    assert r["survived"] is True, r
    if r["is_vaccine"]:
        n_vac += 1
    else:
        if r["because_trained"]:
            n_real_trained += 1
assert n_vac == 2, VA
assert n_real_trained == 1, VA
assert VA[2]["is_vaccine"]      is False, VA[2]
assert VA[2]["because_trained"] is True,  VA[2]
assert d["n_vac_rows_ok"]          == 3, d
assert d["vac_noise_order_ok"]     is True, d
assert d["vac_survived_ok"]        is True, d
assert d["vac_vaccine_count_ok"]   is True, d
assert d["vac_trained_ok"]         is True, d

BB = d["bb"]
want_b = ["safe_mode","experimental_mode","middle_compromise"]
assert [r["allocation"] for r in BB] == want_b, BB
shares = {r["allocation"]: r["share"] for r in BB}
taus   = {r["allocation"]: r["tau"]   for r in BB}
kept   = {r["allocation"]: r["kept"]  for r in BB}
assert abs(shares["safe_mode"] + shares["experimental_mode"] - 1.0) < 1e-3, shares
assert abs(shares["middle_compromise"]) < 1e-3, shares
assert taus["safe_mode"] < taus["experimental_mode"], taus
assert kept["safe_mode"]         is True,  kept
assert kept["experimental_mode"] is True,  kept
assert kept["middle_compromise"] is False, kept
assert d["n_bb_rows_ok"]            == 3, d
assert d["bb_share_sum_ok"]         is True, d
assert d["bb_middle_zero_ok"]       is True, d
assert d["bb_tau_order_ok"]         is True, d
assert d["bb_kept_polarity_ok"]     is True, d

assert 0.0 <= d["sigma_antifragile"] <= 1.0, d
assert d["sigma_antifragile"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v296: non-deterministic" >&2; exit 1; }
