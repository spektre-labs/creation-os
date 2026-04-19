#!/usr/bin/env bash
#
# v281 σ-Jamba — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 layer rows canonical (mamba LINEAR,
#     transformer QUADRATIC, moe SPARSE); all distinct
#   * 4 mixing rows canonical (easy, hard, factual,
#     long); per-label chosen arch canonical;
#     ≥ 2 distinct archs across contexts
#   * 5 memory tiers canonical (engram, mamba, ttt,
#     transformer, moe); all enabled; tier_slot
#     permutation [1,2,3,4,5]
#   * 3 bench metrics canonical (accuracy, latency,
#     throughput); sigma_jamba ≤ sigma_baseline per
#     metric
#   * sigma_jamba in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v281_jamba"
[ -x "$BIN" ] || { echo "v281: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v281", d
assert d["chain_valid"] is True, d

L = d["layer"]
assert len(L) == 3
assert [x["name"] for x in L] == ["mamba", "transformer", "moe"], L
assert [x["complexity"] for x in L] == ["LINEAR", "QUADRATIC", "SPARSE"], L
assert len({x["name"] for x in L}) == 3, L
assert d["n_layer_rows_ok"]      == 3, d
assert d["layer_complexity_ok"]  is True, d
assert d["layer_distinct_ok"]    is True, d

MX = d["mixing"]
assert len(MX) == 4
want = [
    ("easy",    "MAMBA"),
    ("hard",    "TRANSFORMER"),
    ("factual", "MOE"),
    ("long",    "MAMBA"),
]
for (lbl, arch), m in zip(want, MX):
    assert m["label"]  == lbl,  m
    assert m["chosen"] == arch, m
distinct = len({m["chosen"] for m in MX})
assert distinct >= 2, distinct
assert d["n_mixing_rows_ok"]    == 4, d
assert d["n_distinct_arch"]     == distinct, d
assert d["mixing_distinct_ok"]  is True, d

M = d["memory"]
assert len(M) == 5
assert [x["name"] for x in M] == ["engram","mamba","ttt","transformer","moe"], M
assert [x["tier_slot"] for x in M] == [1,2,3,4,5], M
for x in M: assert x["enabled"] is True, x
assert d["n_memory_rows_ok"] == 5, d
assert d["memory_order_ok"]  is True, d

B = d["bench"]
assert len(B) == 3
assert [x["metric"] for x in B] == ["accuracy", "latency", "throughput"], B
for b in B:
    assert 0.0 <= b["sigma_baseline"] <= 1.0, b
    assert 0.0 <= b["sigma_jamba"]    <= 1.0, b
    assert b["sigma_jamba"] <= b["sigma_baseline"] + 1e-9, b
assert d["n_bench_rows_ok"]    == 3, d
assert d["bench_monotone_ok"]  is True, d

assert 0.0 <= d["sigma_jamba"] <= 1.0, d
assert d["sigma_jamba"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v281: non-deterministic" >&2; exit 1; }
