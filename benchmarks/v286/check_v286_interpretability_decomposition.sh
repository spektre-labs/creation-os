#!/usr/bin/env bash
#
# v286 σ-Interpretability — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 decomposition scenarios canonical
#     (low_confidence→entropy, repetitive→repetition,
#     overconfident→calibration, distracted→attention);
#     top_channels form 4 distinct values; every
#     cause is non-empty
#   * 3 attention heads (head_0, head_1, head_2);
#     status CONFIDENT iff σ_head ≤ τ_attn=0.40
#     else UNCERTAIN; both branches fire
#   * 3 counterfactual rows; delta_sigma ==
#     |σ_without − σ_with| within 1e-5;
#     classification CRITICAL iff delta_sigma >
#     δ_critical=0.10 else IRRELEVANT; both branches
#     fire
#   * 3 report rows; trust_percent ∈ [0, 100];
#     explanation_present AND recommendation_present
#     AND eu_article_13_compliant on EVERY row
#   * sigma_interpret in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v286_interpretability"
[ -x "$BIN" ] || { echo "v286: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v286", d
assert d["chain_valid"] is True, d
assert abs(d["tau_attn"]       - 0.40) < 1e-6, d
assert abs(d["delta_critical"] - 0.10) < 1e-6, d

D = d["decomp"]
want_s = ["low_confidence", "repetitive", "overconfident", "distracted"]
want_c = ["entropy", "repetition", "calibration", "attention"]
assert [r["scenario_id"] for r in D] == want_s, D
assert [r["top_channel"] for r in D] == want_c, D
for r in D:
    assert r["cause_nonempty"] is True, r
    assert len(r["cause"]) > 0, r
assert len({r["top_channel"] for r in D}) == 4, D
assert d["n_decomp_rows_ok"]             == 4, d
assert d["decomp_channels_distinct_ok"]  is True, d
assert d["decomp_all_causes_ok"]         is True, d

H = d["head"]
assert [h["name"] for h in H] == ["head_0","head_1","head_2"], H
nc = nu = 0
for h in H:
    exp = "CONFIDENT" if h["sigma_head"] <= d["tau_attn"] else "UNCERTAIN"
    assert h["status"] == exp, h
    if h["status"] == "CONFIDENT": nc += 1
    else:                           nu += 1
assert nc >= 1 and nu >= 1, (nc, nu)
assert d["n_attn_rows_ok"]       == 3, d
assert d["n_attn_confident"]     == nc, d
assert d["n_attn_uncertain"]     == nu, d

C = d["cf"]
assert len(C) == 3
ncr = nir = 0
for r in C:
    want = abs(r["sigma_without"] - r["sigma_with"])
    assert abs(r["delta_sigma"] - want) < 1e-4, r
    exp = "CRITICAL" if r["delta_sigma"] > d["delta_critical"] else "IRRELEVANT"
    assert r["classification"] == exp, r
    if r["classification"] == "CRITICAL":   ncr += 1
    else:                                    nir += 1
assert ncr >= 1 and nir >= 1, (ncr, nir)
assert d["n_cf_rows_ok"]         == 3, d
assert d["n_cf_critical"]        == ncr, d
assert d["n_cf_irrelevant"]      == nir, d
assert d["cf_delta_formula_ok"]  is True, d

R = d["report"]
assert len(R) == 3
for r in R:
    assert 0.0 <= r["trust_percent"] <= 100.0, r
    assert r["explanation_present"]     is True, r
    assert r["recommendation_present"]  is True, r
    assert r["eu_article_13_compliant"] is True, r
assert d["n_report_rows_ok"]           == 3, d
assert d["report_all_compliant_ok"]    is True, d
assert d["report_trust_range_ok"]      is True, d

assert 0.0 <= d["sigma_interpret"] <= 1.0, d
assert d["sigma_interpret"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v286: non-deterministic" >&2; exit 1; }
