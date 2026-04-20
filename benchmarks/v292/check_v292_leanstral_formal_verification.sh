#!/usr/bin/env bash
#
# v292 σ-Leanstral — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 theorem rows canonical (gate_determinism,
#     gate_range, gate_threshold_monotone); all
#     lean4_proved=true
#   * 4 invariant rows canonical (sigma_in_unit_
#     interval, sigma_zero_k_eff_full,
#     sigma_one_k_eff_zero,
#     sigma_monotone_confidence_loss); all holds
#   * 3 cost rows canonical (leanstral $36 <
#     claude $549 < bug_in_prod $10000); strictly
#     increasing
#   * 3 formal-layer rows canonical (frama_c_v138
#     C_CONTRACTS, lean4_v207 THEOREM_PROOFS,
#     leanstral_v292 AI_ASSISTED_PROOFS); 3
#     distinct layers; all enabled
#   * sigma_leanstral in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v292_leanstral"
[ -x "$BIN" ] || { echo "v292: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v292", d
assert d["chain_valid"] is True, d

T = d["theorem"]
want_t = ["gate_determinism","gate_range","gate_threshold_monotone"]
assert [r["name"] for r in T] == want_t, T
for r in T:
    assert r["lean4_proved"] is True, r
assert d["n_theorem_rows_ok"]     == 3, d
assert d["theorem_all_proved_ok"] is True, d

I = d["invariant"]
want_i = ["sigma_in_unit_interval","sigma_zero_k_eff_full",
          "sigma_one_k_eff_zero","sigma_monotone_confidence_loss"]
assert [r["name"] for r in I] == want_i, I
for r in I:
    assert r["holds"] is True, r
assert d["n_invariant_rows_ok"]     == 4, d
assert d["invariant_all_hold_ok"]   is True, d

C = d["cost"]
want_c = [("leanstral",36,"AI_ASSISTED_PROOF"),
          ("claude",549,"AI_GENERIC"),
          ("bug_in_prod",10000,"PROD_BUG")]
for (lbl, c, k), r in zip(want_c, C):
    assert r["label"]          == lbl, r
    assert r["proof_cost_usd"] == c, r
    assert r["kind"]           == k, r
costs = [r["proof_cost_usd"] for r in C]
assert all(costs[i] < costs[i+1] for i in range(len(costs)-1)), costs
assert d["n_cost_rows_ok"]  == 3, d
assert d["cost_monotone_ok"] is True, d

L = d["layer"]
want_l = [("frama_c_v138","C_CONTRACTS"),
          ("lean4_v207","THEOREM_PROOFS"),
          ("leanstral_v292","AI_ASSISTED_PROOFS")]
for (src, ly), r in zip(want_l, L):
    assert r["source"]  == src, r
    assert r["layer"]   == ly, r
    assert r["enabled"] is True, r
assert len({r["layer"] for r in L}) == 3, L
assert d["n_layer_rows_ok"]      == 3, d
assert d["layer_distinct_ok"]    is True, d
assert d["layer_all_enabled_ok"] is True, d

assert 0.0 <= d["sigma_leanstral"] <= 1.0, d
assert d["sigma_leanstral"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v292: non-deterministic" >&2; exit 1; }
