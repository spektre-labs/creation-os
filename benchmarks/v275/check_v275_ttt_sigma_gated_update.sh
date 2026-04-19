#!/usr/bin/env bash
#
# v275 σ-TTT — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 σ-gated update rows; decision LEARN iff
#     σ_update <= τ_update=0.30; both branches
#   * 3 dual-track rows; status cascade
#     (SYNCED <0.15, DIVERGING <0.50, RESET else);
#     all three branches fire
#   * 6 sliding-window tokens; evict_rank is a
#     permutation of [1..6] matching descending σ
#     (rank 1 = highest σ evicted first)
#   * 2 validation citations: v124_sigma_continual
#     (living-weights thesis) and ttt_e2e_2025
#     (Stanford/NVIDIA); both present; distinct sources
#   * sigma_ttt in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v275_ttt"
[ -x "$BIN" ] || { echo "v275: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v275", d
assert d["chain_valid"] is True, d
assert abs(d["tau_update"] - 0.30) < 1e-6, d
assert abs(d["tau_sync"]   - 0.15) < 1e-6, d
assert abs(d["tau_reset"]  - 0.50) < 1e-6, d

U = d["update"]
assert len(U) == 4
nl = ns = 0
for u in U:
    exp = "LEARN" if u["sigma_update"] <= d["tau_update"] else "SKIP"
    assert u["decision"] == exp, u
    if u["decision"] == "LEARN": nl += 1
    else:                          ns += 1
assert nl >= 1 and ns >= 1, (nl, ns)
assert d["n_update_rows_ok"] == 4, d
assert d["n_update_learn"]   == nl, d
assert d["n_update_skip"]    == ns, d

T = d["dualtrack"]
assert len(T) == 3
ns_ = nd_ = nr_ = 0
for t in T:
    if t["sigma_drift"] < d["tau_sync"]:       exp = "SYNCED"
    elif t["sigma_drift"] < d["tau_reset"]:    exp = "DIVERGING"
    else:                                        exp = "RESET"
    assert t["status"] == exp, t
    if t["status"] == "SYNCED":    ns_ += 1
    if t["status"] == "DIVERGING": nd_ += 1
    if t["status"] == "RESET":     nr_ += 1
assert ns_ >= 1 and nd_ >= 1 and nr_ >= 1, (ns_, nd_, nr_)
assert d["n_dualtrack_rows_ok"] == 3, d
assert d["n_dt_synced"]   == ns_, d
assert d["n_dt_diverging"]== nd_, d
assert d["n_dt_reset"]    == nr_, d

W = d["window"]
assert len(W) == 6
ranks = [w["evict_rank"] for w in W]
assert sorted(ranks) == [1,2,3,4,5,6], ranks
# descending-σ order: higher σ ⇒ lower evict_rank
for i in range(len(W)):
    for j in range(len(W)):
        if i == j: continue
        if W[i]["sigma_token"] > W[j]["sigma_token"]:
            assert W[i]["evict_rank"] < W[j]["evict_rank"], (W[i], W[j])
assert d["n_window_rows_ok"] == 6, d
assert d["window_order_ok"]  is True, d

V = d["validation"]
assert len(V) == 2
want = ["v124_sigma_continual", "ttt_e2e_2025"]
assert [v["source"] for v in V] == want, V
for v in V:
    assert v["present"] is True, v
    assert len(v["evidence"]) > 0, v
assert V[0]["source"] != V[1]["source"], V
assert d["n_validation_rows_ok"]    == 2,   d
assert d["validation_distinct_ok"]  is True, d

assert 0.0 <= d["sigma_ttt"] <= 1.0, d
assert d["sigma_ttt"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v275: non-deterministic" >&2; exit 1; }
