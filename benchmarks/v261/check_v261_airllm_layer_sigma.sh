#!/usr/bin/env bash
#
# v261 σ-AirLLM — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 8 layers with index 0..7 strictly ascending,
#     sigma_layer in [0,1], precision_bits follows the
#     σ-driven rule byte-for-byte:
#       σ <= 0.20 → 4, 0.20 < σ <= 0.40 → 8, σ > 0.40 → 16
#   * exactly one is_problem==true AND it matches the
#     argmax of sigma_layer AND problem_index matches
#   * 4 hardware backends (cuda_4gb_gpu, cpu_only,
#     macos_mps, linux_cuda), every supported,
#     min_ram_gb >= 4
#   * 3 tradeoff regimes (slow, aircos, human); aircos
#     has the STRICTLY highest tokens_per_s AND
#     abstain_pct > 0 (gate has teeth); tradeoff_ok
#   * sigma_airllm in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v261_airllm"
[ -x "$BIN" ] || { echo "v261: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v261", d
assert d["chain_valid"] is True, d

def want_bits(s):
    if s <= 0.20: return 4
    if s <= 0.40: return 8
    return 16

layers = d["layers"]
assert len(layers) == 8, d
prev_idx = -1
problem_cnt = 0
max_s = -1.0; max_i = -1
for l in layers:
    assert l["index"] == prev_idx + 1, l
    prev_idx = l["index"]
    assert 0.0 <= l["sigma_layer"] <= 1.0, l
    assert l["precision_bits"] == want_bits(l["sigma_layer"]), l
    if l["sigma_layer"] > max_s:
        max_s = l["sigma_layer"]; max_i = l["index"]
    if l["is_problem"]: problem_cnt += 1
assert problem_cnt == 1, d
assert layers[max_i]["is_problem"] is True, d
assert d["problem_index"] == max_i, d
assert d["n_layers_ok"]    == 8, d
assert d["n_precision_ok"] == 8, d

bn = [b["name"] for b in d["backends"]]
assert bn == ["cuda_4gb_gpu","cpu_only","macos_mps","linux_cuda"], bn
for b in d["backends"]:
    assert b["supported"] is True, b
    assert b["min_ram_gb"] >= 4, b
assert d["n_backends_ok"] == 4, d

tr = d["tradeoff"]
regimes = [t["regime"] for t in tr]
assert set(regimes) == {"slow","aircos","human"}, regimes
aircos = [t for t in tr if t["regime"] == "aircos"][0]
for t in tr:
    if t["regime"] == "aircos": continue
    assert t["tokens_per_s"] < aircos["tokens_per_s"], (t, aircos)
assert aircos["abstain_pct"] > 0, aircos
assert d["tradeoff_ok"] is True, d

assert 0.0 <= d["sigma_airllm"] <= 1.0, d
assert d["sigma_airllm"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v261: non-deterministic" >&2; exit 1; }
