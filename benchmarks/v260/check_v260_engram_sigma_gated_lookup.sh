#!/usr/bin/env bash
#
# v260 σ-Engram — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * parameter split: static_pct in [20,25],
#     dynamic_pct in [75,80], sum==100, split_ok
#   * 5 static lookups with hash != 0, hit==true,
#     sigma_result in [0,1], lookup_ns <= 100 (O(1) DRAM)
#   * 3 dynamic rows: experts_activated > 0,
#     sigma_result in [0,1]
#   * 4 gate fixtures: decision matches σ vs
#     τ_fact=0.30 (≤ → USE, > → VERIFY); >=1 USE AND
#     >=1 VERIFY
#   * long context: hit_rate_pct==97, miss_rate_pct==3,
#     miss_flagged_by_sigma==true, longctx.ok
#   * sigma_engram in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v260_engram"
[ -x "$BIN" ] || { echo "v260: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v260", d
assert d["chain_valid"] is True, d
assert abs(d["tau_fact"] - 0.30) < 1e-6, d
assert d["lookup_ns_budget"] == 100, d

sp = d["split"]
assert 20 <= sp["static_pct"]  <= 25, sp
assert 75 <= sp["dynamic_pct"] <= 80, sp
assert sp["static_pct"] + sp["dynamic_pct"] == 100, sp
assert sp["split_ok"] is True, sp

assert len(d["statics"]) == 5, d
for r in d["statics"]:
    assert int(r["hash"], 16) != 0, r
    assert r["hit"] is True, r
    assert 0.0 <= r["sigma_result"] <= 1.0, r
    assert r["lookup_ns"] <= 100, r
assert d["n_static_ok"] == 5, d

assert len(d["dynamics"]) == 3, d
for r in d["dynamics"]:
    assert r["experts_activated"] > 0, r
    assert 0.0 <= r["sigma_result"] <= 1.0, r
assert d["n_dynamic_ok"] == 3, d

tau = d["tau_fact"]
for g in d["gate"]:
    exp = "USE" if g["sigma_result"] <= tau else "VERIFY"
    assert g["decision"] == exp, g
assert d["n_use"]    >= 1, d
assert d["n_verify"] >= 1, d
assert d["n_use"] + d["n_verify"] == 4, d

lc = d["longctx"]
assert lc["hit_rate_pct"]  == 97, lc
assert lc["miss_rate_pct"] == 3,  lc
assert lc["miss_flagged_by_sigma"] is True, lc
assert lc["ok"] is True, lc

assert 0.0 <= d["sigma_engram"] <= 1.0, d
assert d["sigma_engram"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v260: non-deterministic" >&2; exit 1; }
