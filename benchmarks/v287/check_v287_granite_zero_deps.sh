#!/usr/bin/env bash
#
# v287 σ-Granite — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 dependency rows canonical (libc/posix/pthreads
#     ALLOW with in_kernel=true; npm/pip/cargo FORBID
#     with in_kernel=false)
#   * 3 language standards canonical (C89/C99/C++)
#     with exactly 2 allowed AND 1 forbidden (C++)
#   * 5 platform targets canonical (linux/macos/
#     bare_metal/rtos/cortex_m) with kernel_works
#     AND ifdef_only_at_edges on EVERY row
#   * 3 vendoring rows canonical (vendored_copy
#     allowed; external_linkage AND
#     supply_chain_trust forbidden)
#   * sigma_granite in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v287_granite"
[ -x "$BIN" ] || { echo "v287: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v287", d
assert d["chain_valid"] is True, d

D = d["dep"]
assert [r["name"] for r in D] == ["libc","posix","pthreads","npm","pip","cargo"], D
for r in D[:3]:
    assert r["verdict"] == "ALLOW",  r
    assert r["in_kernel"] is True,   r
for r in D[3:]:
    assert r["verdict"] == "FORBID", r
    assert r["in_kernel"] is False,  r
assert d["n_dep_rows_ok"]          == 6, d
assert d["dep_allow_polarity_ok"]  is True, d
assert d["dep_forbid_polarity_ok"] is True, d

S = d["stdlang"]
assert [r["name"] for r in S] == ["C89","C99","C++"], S
assert S[0]["allowed"] is True
assert S[1]["allowed"] is True
assert S[2]["allowed"] is False
assert d["n_std_rows_ok"] == 3, d
assert d["std_count_ok"]  is True, d

P = d["platform"]
assert [r["name"] for r in P] == ["linux","macos","bare_metal","rtos","cortex_m"], P
for r in P:
    assert r["kernel_works"]         is True, r
    assert r["ifdef_only_at_edges"]  is True, r
assert d["n_platform_rows_ok"] == 5, d
assert d["platform_works_ok"]  is True, d
assert d["platform_ifdef_ok"]  is True, d

V = d["vendor"]
assert [r["name"] for r in V] == ["vendored_copy","external_linkage","supply_chain_trust"], V
assert V[0]["allowed_in_kernel"] is True
assert V[1]["allowed_in_kernel"] is False
assert V[2]["allowed_in_kernel"] is False
assert d["n_vendor_rows_ok"]   == 3, d
assert d["vendor_polarity_ok"] is True, d

assert 0.0 <= d["sigma_granite"] <= 1.0, d
assert d["sigma_granite"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v287: non-deterministic" >&2; exit 1; }
