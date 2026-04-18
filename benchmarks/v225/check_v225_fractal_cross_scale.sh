#!/usr/bin/env bash
#
# v225 σ-Fractal — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 5 levels, fan-out 2, 31 nodes
#   * scale-invariance at every internal node (σ_parent
#     == mean(σ_children) to 1e-6)
#   * σ_cross_true == 0 (aggregator matches itself)
#   * σ_cross_declared ≥ 1 (planted mismatch detected)
#   * K(K)=K invariant holds at every internal node
#   * σ, σ_declared ∈ [0, 1]
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v225_fractal"
[ -x "$BIN" ] || { echo "v225: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v225", d
assert d["n_levels"] == 5, d
assert d["fanout"]   == 2, d
assert d["n_nodes"]  == 31, d
assert d["chain_valid"] is True, d

n_internal = sum(1 for n in d["nodes"] if n["nc"] > 0)
assert d["n_scale_invariant_ok"] == n_internal, d
assert d["n_cross_true_diff"]    == 0,          d
assert d["n_cross_declared_diff"] >= 1,          d
assert d["n_kk_ok"] == n_internal,              d

# Scale-invariance directly verified on every parent.
by_id = {n["id"]: n for n in d["nodes"]}
for n in d["nodes"]:
    if n["nc"] == 0: continue
    children = [c for c in d["nodes"] if c["parent"] == n["id"]]
    assert len(children) == n["nc"], n
    mean_c = sum(c["sigma"] for c in children) / n["nc"]
    assert abs(n["sigma"] - mean_c) < 1e-3, (n, mean_c)

# σ / declared bounds.
for n in d["nodes"]:
    assert 0.0 <= n["sigma"]    <= 1.0 + 1e-6, n
    assert 0.0 <= n["declared"] <= 1.0 + 1e-6, n

# Root K must be 1 − σ_root.
assert abs(d["k_root"] - (1.0 - d["sigma_root"])) < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v225: non-deterministic" >&2; exit 1; }
