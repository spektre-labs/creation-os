#!/usr/bin/env bash
#
# v278 σ-Recursive-Self-Improve — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 calibration epochs with strictly decreasing
#     sigma_calibration_err; last epoch <= 0.05
#   * 3 arch configurations (6/8/12 channels); chosen
#     == argmax(aurcc); exactly one chosen; >=2 distinct
#     aurcc values
#   * 3 threshold rows canonical (code=0.20,
#     creative=0.50, medical=0.15); all tau in (0, 1);
#     >=2 distinct tau values
#   * 3 Gödel rows; action SELF_CONFIDENT iff
#     sigma_goedel <= tau_goedel=0.40 else
#     CALL_PROCONDUCTOR; both branches fire
#   * sigma_rsi in [0, 1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v278_rsi"
[ -x "$BIN" ] || { echo "v278: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v278", d
assert d["chain_valid"] is True, d
assert abs(d["tau_goedel"] - 0.40) < 1e-6, d

C = d["calibration"]
assert len(C) == 4
for i in range(1, len(C)):
    assert C[i]["sigma_calibration_err"] < C[i-1]["sigma_calibration_err"], (i, C)
assert C[-1]["sigma_calibration_err"] <= 0.05, C[-1]
assert d["n_calibration_rows_ok"]   == 4, d
assert d["calibration_monotone_ok"] is True, d
assert d["calibration_anchor_ok"]   is True, d

A = d["arch"]
assert len(A) == 3
assert [a["n_channels"] for a in A] == [6, 8, 12], A
best_idx = max(range(len(A)), key=lambda i: A[i]["aurcc"])
assert d["arch_chosen_idx"] == best_idx, d
n_chosen = sum(1 for a in A if a["chosen"])
assert n_chosen == 1, A
assert A[best_idx]["chosen"] is True, A
distinct_aurcc = len(set(round(a["aurcc"], 4) for a in A))
assert distinct_aurcc >= 2, A
assert d["n_arch_rows_ok"]    == 3, d
assert d["arch_chosen_ok"]    is True, d
assert d["arch_distinct_ok"]  is True, d
assert d["n_distinct_aurcc"]  == distinct_aurcc, d

TH = d["thresholds"]
assert [t["domain"] for t in TH] == ["code","creative","medical"], TH
assert abs(TH[0]["tau"] - 0.20) < 1e-4, TH[0]
assert abs(TH[1]["tau"] - 0.50) < 1e-4, TH[1]
assert abs(TH[2]["tau"] - 0.15) < 1e-4, TH[2]
for t in TH:
    assert 0.0 < t["tau"] < 1.0, t
distinct_tau = len(set(round(t["tau"], 4) for t in TH))
assert distinct_tau >= 2, TH
assert d["n_threshold_rows_ok"]    == 3, d
assert d["threshold_canonical_ok"] is True, d
assert d["threshold_distinct_ok"]  is True, d
assert d["n_distinct_tau"]         == distinct_tau, d

G = d["goedel"]
assert len(G) == 3
n_s = n_p = 0
for g in G:
    exp = "SELF_CONFIDENT" if g["sigma_goedel"] <= d["tau_goedel"] else "CALL_PROCONDUCTOR"
    assert g["action"] == exp, g
    if g["action"] == "SELF_CONFIDENT":    n_s += 1
    if g["action"] == "CALL_PROCONDUCTOR": n_p += 1
assert n_s >= 1 and n_p >= 1, (n_s, n_p)
assert d["n_goedel_rows_ok"] == 3, d
assert d["n_goedel_self"]    == n_s, d
assert d["n_goedel_procon"]  == n_p, d

assert 0.0 <= d["sigma_rsi"] <= 1.0, d
assert d["sigma_rsi"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v278: non-deterministic" >&2; exit 1; }
