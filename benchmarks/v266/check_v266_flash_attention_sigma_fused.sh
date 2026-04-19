#!/usr/bin/env bash
#
# v266 σ-Flash — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 8 attention heads (indices 0..7), all fused,
#     sigma_head in [0,1], overhead_pct in [0,1)
#     (strict sub-1 % σ-fusion cost)
#   * 3 platforms canonical (cuda_sm90, metal_m4,
#     neon_arm64), all supported, latency_ns > 0,
#     sigma_fused; n_platforms_ok == 3
#   * 6 KV entries, sigma_kv in [0,1]; evict_rank is a
#     permutation of [1..6] AND descending σ order
#     (rank 1 = max σ, rank 6 = min σ); kv_order_ok
#   * pruning: kept_tokens_before == kept_tokens_after
#     AND effective_ctx_k_after > effective_ctx_k_before
#     AND pruning_ok
#   * sigma_flash in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v266_flash"
[ -x "$BIN" ] || { echo "v266: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v266", d
assert d["chain_valid"] is True, d

heads = d["heads"]
assert len(heads) == 8, heads
for i, h in enumerate(heads):
    assert h["index"] == i, h
    assert h["fused"] is True, h
    assert 0.0 <= h["sigma_head"] <= 1.0, h
    assert 0.0 <= h["overhead_pct"] < 1.0, h
assert d["n_heads_ok"] == 8, d

pl = d["platforms"]
assert [p["backend"] for p in pl] == ["cuda_sm90","metal_m4","neon_arm64"], pl
for p in pl:
    assert p["supported"]  is True, p
    assert p["latency_ns"] > 0,     p
    assert p["sigma_fused"] is True, p
assert d["n_platforms_ok"] == 3, d

kv = d["kv"]
assert len(kv) == 6
ranks = sorted([k["evict_rank"] for k in kv])
assert ranks == [1,2,3,4,5,6], kv
by_rank = sorted(kv, key=lambda k: k["evict_rank"])
for i in range(1, 6):
    assert by_rank[i-1]["sigma_kv"] >= by_rank[i]["sigma_kv"], by_rank
for k in kv:
    assert 0.0 <= k["sigma_kv"] <= 1.0, k
assert d["n_kv_ok"] == 6, d
assert d["kv_order_ok"] is True, d

pr = d["pruning"]
assert pr["kept_tokens_before"] == pr["kept_tokens_after"], pr
assert pr["effective_ctx_k_after"] > pr["effective_ctx_k_before"], pr
assert pr["pruning_ok"] is True, pr

assert 0.0 <= d["sigma_flash"] <= 1.0, d
assert d["sigma_flash"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v266: non-deterministic" >&2; exit 1; }
