#!/usr/bin/env bash
#
# v267 σ-Mamba — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 backends canonical (mamba, rwkv, transformer);
#     mamba/rwkv exponent==1 AND transformer exponent==2;
#     mamba/rwkv throughput_rel > transformer throughput_rel
#   * 4 route fixtures (τ_mamba=0.40); decision matches
#     σ≤τ→mamba else transformer; >=1 mamba AND >=1
#     transformer
#   * 8 hybrid layers alternating mamba/transformer;
#     4 mamba + 4 transformer; sigma_arch in [0,1]
#   * 3 tasks with sigma_chosen ≤ sigma_rival each;
#     chosen backend set has >= 2 distinct entries
#   * sigma_mamba_kernel in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v267_mamba"
[ -x "$BIN" ] || { echo "v267: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v267", d
assert d["chain_valid"] is True, d
assert abs(d["tau_mamba"] - 0.40) < 1e-6, d

b = d["backends"]
assert [x["name"] for x in b] == ["mamba","rwkv","transformer"], b
assert b[0]["exponent"] == 1, b
assert b[1]["exponent"] == 1, b
assert b[2]["exponent"] == 2, b
assert b[0]["throughput_rel"] > b[2]["throughput_rel"], b
assert b[1]["throughput_rel"] > b[2]["throughput_rel"], b
assert d["n_backends_ok"] == 3, d

tau = d["tau_mamba"]
for r in d["routes"]:
    exp = "mamba" if r["sigma_mamba"] <= tau else "transformer"
    assert r["backend"] == exp, r
    assert 0.0 <= r["sigma_mamba"] <= 1.0, r
assert d["n_routes_ok"] == 4, d
assert d["n_mamba_routes"] >= 1 and d["n_trans_routes"] >= 1, d

L = d["layers"]
assert len(L) == 8
for i, l in enumerate(L):
    exp = "mamba" if i % 2 == 0 else "transformer"
    assert l["arch"] == exp, l
    assert 0.0 <= l["sigma_arch"] <= 1.0, l
    assert l["idx"] == i, l
assert d["n_layers_ok"] == 8, d
assert d["n_mamba_layers"] == 4, d
assert d["n_trans_layers"] == 4, d

T = d["tasks"]
assert len(T) == 3
for t in T:
    assert t["sigma_chosen"] <= t["sigma_rival"], t
    assert 0.0 <= t["sigma_chosen"] <= 1.0, t
    assert 0.0 <= t["sigma_rival"]  <= 1.0, t
chosen = {t["chosen"] for t in T}
assert len(chosen) >= 2, chosen
assert d["n_tasks_ok"] == 3, d
assert d["n_distinct_chosen"] >= 2, d

assert 0.0 <= d["sigma_mamba_kernel"] <= 1.0, d
assert d["sigma_mamba_kernel"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v267: non-deterministic" >&2; exit 1; }
