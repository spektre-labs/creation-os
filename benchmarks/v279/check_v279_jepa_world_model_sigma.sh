#!/usr/bin/env bash
#
# v279 σ-JEPA — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 prediction rows; decision ACT iff
#     σ_prediction <= τ_predict=0.30 else OBSERVE;
#     both branches fire
#   * 3 latent checkpoints canonical order
#     (early, mid, late); entropy_z and sigma_latent
#     both strictly decreasing; |entropy_z −
#     sigma_latent| ≤ 0.05 per row
#   * 2 loss terms canonical (prediction, regularizer),
#     distinct, weights sum to 1.0
#   * 2 validation citations (lecun_jepa_2022 and
#     leworldmodel_2026_03) both present, distinct
#   * sigma_jepa in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v279_jepa"
[ -x "$BIN" ] || { echo "v279: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v279", d
assert d["chain_valid"] is True, d
assert abs(d["tau_predict"]  - 0.30) < 1e-6, d
assert abs(d["converge_eps"] - 0.05) < 1e-6, d

P = d["predict"]
assert len(P) == 4
na = no = 0
for p in P:
    exp = "ACT" if p["sigma_prediction"] <= d["tau_predict"] else "OBSERVE"
    assert p["decision"] == exp, p
    if p["decision"] == "ACT":     na += 1
    else:                           no += 1
assert na >= 1 and no >= 1, (na, no)
assert d["n_predict_rows_ok"] == 4, d
assert d["n_predict_act"]     == na, d
assert d["n_predict_observe"] == no, d

L = d["latent"]
assert len(L) == 3
assert [x["label"] for x in L] == ["early", "mid", "late"], L
for i in range(1, 3):
    assert L[i]["entropy_z"]    < L[i-1]["entropy_z"],    L
    assert L[i]["sigma_latent"] < L[i-1]["sigma_latent"], L
for row in L:
    assert abs(row["entropy_z"] - row["sigma_latent"]) <= d["converge_eps"] + 1e-6, row
assert d["n_latent_rows_ok"]          == 3,   d
assert d["latent_monotone_entropy_ok"] is True, d
assert d["latent_monotone_sigma_ok"]   is True, d
assert d["latent_converge_ok"]         is True, d

LS = d["loss"]
assert len(LS) == 2
assert [x["name"] for x in LS] == ["prediction", "regularizer"], LS
for l in LS:
    assert l["enabled"] is True, l
    assert 0.0 < l["weight"] < 1.0, l
assert abs(sum(l["weight"] for l in LS) - 1.0) <= 1e-3, LS
assert d["n_loss_rows_ok"]     == 2,   d
assert d["loss_distinct_ok"]   is True, d
assert d["loss_weights_ok"]    is True, d

V = d["validation"]
assert len(V) == 2
want = ["lecun_jepa_2022", "leworldmodel_2026_03"]
assert [v["source"] for v in V] == want, V
for v in V:
    assert v["present"] is True, v
    assert len(v["evidence"]) > 0, v
assert V[0]["source"] != V[1]["source"], V
assert d["n_validation_rows_ok"]   == 2,   d
assert d["validation_distinct_ok"] is True, d

assert 0.0 <= d["sigma_jepa"] <= 1.0, d
assert d["sigma_jepa"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v279: non-deterministic" >&2; exit 1; }
