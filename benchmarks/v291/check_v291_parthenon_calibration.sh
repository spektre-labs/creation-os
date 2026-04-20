#!/usr/bin/env bash
#
# v291 σ-Parthenon — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 calibration rows canonical (medical/code/
#     creative) with distinct verdicts (ABSTAIN/
#     CAUTIOUS/SAFE) and shared sigma_sample=0.30
#   * 3 perception rows canonical
#     (sigma=0.05/0.15/0.50) with
#     ratio_denominator == round(1/sigma);
#     explanation_present on every row
#   * 3 bias rows canonical (overconfident +0.10,
#     underconfident -0.10, calibrated 0.00);
#     sigma_corrected == sigma_raw + bias_offset;
#     residual_bias <= bias_budget (0.02) on every row
#     — matches merge-gate "bias < 0.02 kaikissa
#     domaineissa"
#   * 3 entasis rows canonical with
#     sigma_clamped == clamp(sigma_in, 0.02, 0.98)
#   * sigma_parthenon in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v291_parthenon"
[ -x "$BIN" ] || { echo "v291: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v291", d
assert d["chain_valid"] is True, d
assert abs(d["bias_budget"]         - 0.02) < 1e-6, d
assert abs(d["lower_bound"]         - 0.02) < 1e-6, d
assert abs(d["upper_bound"]         - 0.98) < 1e-6, d
assert abs(d["shared_sigma_sample"] - 0.30) < 1e-6, d

C = d["calib"]
want_c = [("medical","ABSTAIN"), ("code","CAUTIOUS"),
          ("creative","SAFE")]
for (dom, v), r in zip(want_c, C):
    assert r["domain"]  == dom, r
    assert r["verdict"] == v, r
    assert abs(r["sigma_sample"] - 0.30) < 1e-6, r
assert len({r["verdict"] for r in C}) == 3, C
assert d["n_calib_rows_ok"]           == 3, d
assert d["calib_verdict_distinct_ok"] is True, d
assert d["calib_sample_shared_ok"]    is True, d

P = d["percept"]
want_p = [(0.05, 20), (0.15, 7), (0.50, 2)]
for (sg, den), r in zip(want_p, P):
    assert abs(r["sigma"] - sg) < 1e-5, r
    assert r["ratio_denominator"]   == den, r
    assert r["explanation_present"] is True, r
    assert abs(round(1.0/r["sigma"]) - r["ratio_denominator"]) < 0.5, r
assert d["n_percept_rows_ok"]     == 3, d
assert d["percept_ratio_ok"]       is True, d
assert d["percept_explanation_ok"] is True, d

B = d["bias"]
want_b = [
    ("overconfident",  0.20, +0.10, 0.30),
    ("underconfident", 0.60, -0.10, 0.50),
    ("calibrated",     0.40,  0.00, 0.40),
]
for (t, r_raw, off, corr), r in zip(want_b, B):
    assert r["bias_type"] == t, r
    assert abs(r["sigma_raw"]       - r_raw) < 1e-5, r
    assert abs(r["bias_offset"]     - off)   < 1e-5, r
    assert abs(r["sigma_corrected"] - corr)  < 1e-5, r
    assert abs(r["sigma_corrected"] - (r["sigma_raw"] + r["bias_offset"])) < 1e-5, r
    assert r["residual_bias"] <= d["bias_budget"] + 1e-9, r
assert B[0]["bias_offset"] > 0 and B[1]["bias_offset"] < 0
assert abs(B[2]["bias_offset"]) < 1e-6
assert d["n_bias_rows_ok"]    == 3, d
assert d["bias_formula_ok"]   is True, d
assert d["bias_polarity_ok"]  is True, d
assert d["bias_budget_ok"]    is True, d

E = d["entasis"]
def clamp(x, lo, hi): return max(lo, min(hi, x))
want_e = [(0.005, 0.02), (0.995, 0.98), (0.500, 0.50)]
for (sin, sout), r in zip(want_e, E):
    assert abs(r["sigma_in"]      - sin)  < 1e-5, r
    assert abs(r["sigma_clamped"] - sout) < 1e-5, r
    assert abs(r["sigma_clamped"] - clamp(r["sigma_in"],
               d["lower_bound"], d["upper_bound"])) < 1e-5, r
assert d["n_entasis_rows_ok"]        == 3, d
assert d["entasis_clamp_formula_ok"] is True, d

assert 0.0 <= d["sigma_parthenon"] <= 1.0, d
assert d["sigma_parthenon"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v291: non-deterministic" >&2; exit 1; }
