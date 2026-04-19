#!/usr/bin/env bash
#
# v276 σ-Gated-DeltaNet — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 2 canonical backends (deltanet exp=1 gate=true,
#     transformer exp=2 gate=false); deltanet
#     throughput_rel > transformer throughput_rel
#   * 4 σ-gate fixtures; decision LINEAR iff σ_gate <=
#     τ_gate=0.35 else FALLBACK_FULL; both branches
#   * 3 combo components canonical (deltanet, ttt,
#     sigma_gate) each enabled with layer_slot 1..3
#   * 3 tri-backend tasks; chosen == argmin(σ); each
#     sigma_chosen <= sigma_rival; >=2 distinct chosen
#     backends across tasks
#   * sigma_deltanet in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v276_gated_deltanet"
[ -x "$BIN" ] || { echo "v276: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v276", d
assert d["chain_valid"] is True, d
assert abs(d["tau_gate"] - 0.35) < 1e-6, d

B = d["backends"]
assert [b["name"] for b in B] == ["deltanet","transformer"], B
assert B[0]["exponent"] == 1, B
assert B[1]["exponent"] == 2, B
assert B[0]["gate_mechanism"] is True,  B
assert B[1]["gate_mechanism"] is False, B
assert B[0]["throughput_rel"] > B[1]["throughput_rel"], B
assert d["n_backend_rows_ok"]     == 2, d
assert d["backend_exponents_ok"]  is True, d
assert d["backend_throughput_ok"] is True, d

G = d["gate"]
assert len(G) == 4
n_l = n_f = 0
for g in G:
    exp = "LINEAR" if g["sigma_gate"] <= d["tau_gate"] else "FALLBACK_FULL"
    assert g["decision"] == exp, g
    if g["decision"] == "LINEAR":        n_l += 1
    if g["decision"] == "FALLBACK_FULL": n_f += 1
assert n_l >= 1 and n_f >= 1, (n_l, n_f)
assert d["n_gate_rows_ok"]  == 4, d
assert d["n_gate_linear"]   == n_l, d
assert d["n_gate_fallback"] == n_f, d

C = d["combo"]
assert [c["component"] for c in C] == ["deltanet","ttt","sigma_gate"], C
for i, c in enumerate(C):
    assert c["enabled"] is True, c
    assert c["layer_slot"] == i + 1, c
assert d["n_combo_rows_ok"] == 3, d
assert d["combo_order_ok"]  is True, d

T = d["tasks"]
assert len(T) == 3
chosen_set = set()
for t in T:
    sm, sd, st = t["sigma_mamba"], t["sigma_deltanet"], t["sigma_ttt"]
    best = min(zip(("mamba","deltanet","ttt"), (sm, sd, st)), key=lambda kv: kv[1])
    assert t["chosen"] == best[0], t
    assert abs(t["sigma_chosen"] - best[1]) < 1e-5, t
    rival_vals = [sv for nm, sv in (("mamba",sm),("deltanet",sd),("ttt",st)) if nm != t["chosen"]]
    assert abs(t["sigma_rival"] - min(rival_vals)) < 1e-5, t
    assert t["sigma_chosen"] <= t["sigma_rival"], t
    chosen_set.add(t["chosen"])
assert len(chosen_set) >= 2, chosen_set
assert d["n_task_rows_ok"]   == 3, d
assert d["n_task_chosen_ok"] == 3, d
assert d["n_distinct_chosen"] == len(chosen_set), d

assert 0.0 <= d["sigma_deltanet"] <= 1.0, d
assert d["sigma_deltanet"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v276: non-deterministic" >&2; exit 1; }
