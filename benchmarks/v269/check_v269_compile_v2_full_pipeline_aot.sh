#!/usr/bin/env bash
#
# v269 σ-Compile-v2 — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 pipeline stages canonical (tokenize, embed,
#     attention, ffn, sigma_gate, detokenize); every
#     aot_compiled && native && latency_ns > 0
#   * 4 platform targets canonical (m4_apple_silicon,
#     rpi5_arm64, gpu_4gb_speculative, x86_avx512);
#     every tok_per_s >= budget_tok_per_s AND
#     meets_budget; n_targets_ok == 4
#   * 4 PGO rows; optimization == "aggressive" iff
#     hotpath_fraction >= 0.20; >=1 aggressive AND
#     >=1 space
#   * 6 elim rows; elided iff sigma_profile < 0.05;
#     >=1 elided AND >=1 kept
#   * sigma_compile_v2 in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v269_compile_v2"
[ -x "$BIN" ] || { echo "v269: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v269", d
assert d["chain_valid"] is True, d

S = d["stages"]
want = ["tokenize","embed","attention","ffn","sigma_gate","detokenize"]
assert [s["stage"] for s in S] == want, S
for s in S:
    assert s["aot_compiled"] is True, s
    assert s["native"]       is True, s
    assert s["latency_ns"]   >  0,    s
assert d["n_stages_ok"] == 6, d

T = d["targets"]
tw = ["m4_apple_silicon","rpi5_arm64","gpu_4gb_speculative","x86_avx512"]
assert [t["target"] for t in T] == tw, T
for t in T:
    assert t["supported"] is True, t
    assert t["tok_per_s"] >= t["budget_tok_per_s"], t
    assert t["meets_budget"] is True, t
assert d["n_targets_ok"] == 4, d

P = d["pgo"]
assert len(P) == 4
n_a = n_s = 0
for p in P:
    exp = "aggressive" if p["hotpath_fraction"] >= 0.20 else "space"
    assert p["optimization"] == exp, p
    assert 0.0 <= p["hotpath_fraction"] <= 1.0, p
    if p["optimization"] == "aggressive": n_a += 1
    else:                                  n_s += 1
assert n_a >= 1 and n_s >= 1, (n_a, n_s)
assert d["n_pgo_ok"] == 4, d
assert d["n_pgo_aggressive"] == n_a, d
assert d["n_pgo_space"]      == n_s, d

E = d["elim"]
assert len(E) == 6
n_el = n_k = 0
for e in E:
    exp = e["sigma_profile"] < 0.05
    assert e["elided"] == exp, e
    assert 0.0 <= e["sigma_profile"] <= 1.0, e
    if e["elided"]: n_el += 1
    else:            n_k  += 1
assert n_el >= 1 and n_k >= 1, (n_el, n_k)
assert d["n_elim_ok"] == 6, d
assert d["n_elided"] == n_el, d
assert d["n_kept"]   == n_k,  d

assert 0.0 <= d["sigma_compile_v2"] <= 1.0, d
assert d["sigma_compile_v2"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v269: non-deterministic" >&2; exit 1; }
