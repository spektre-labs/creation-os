#!/usr/bin/env bash
#
# v239 σ-Compose-Runtime — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_requests == 5; one deliberately over-budget
#   * per accepted request: closure_complete AND topo_ok AND
#     every parent id in DEPS is present in active AND activated
#     at a strictly earlier tick than its child
#   * n_active <= max_kernels for accepted requests
#   * over_budget ⇔ !accepted; exactly one over-budget request
#   * sigma_activation in [0, 1] AND == n_active / max_kernels
#     (within 1e-4) for accepted requests
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v239_runtime"
[ -x "$BIN" ] || { echo "v239: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v239", d
assert d["chain_valid"] is True, d
assert d["n_requests"] == 5, d

DEPS = [
    (150, 114), (114, 106), (115, 106), (111, 101), (147, 111),
    (135, 111), (112, 106), (113, 112), (121, 111), (106, 101),
    (101,  29),
]
parents_of = {}
for c, p in DEPS:
    parents_of.setdefault(c, []).append(p)

n_over = 0
n_accepted = 0
for r in d["requests"]:
    if r["over_budget"]:
        n_over += 1
        assert r["accepted"] is False, r
        continue
    n_accepted += 1
    assert r["accepted"] is True, r
    assert r["closure_complete"] is True, r
    assert r["topo_ok"] is True, r
    active_ids    = [a["id"] for a in r["active"]]
    ticks_by_id   = {a["id"]: a["activated_at_tick"] for a in r["active"]}
    assert r["n_active"] == len(active_ids), r
    assert r["n_active"] <= r["max_kernels"], r
    for kid in active_ids:
        for p in parents_of.get(kid, []):
            assert p in ticks_by_id, (kid, p, r)
            assert ticks_by_id[p] < ticks_by_id[kid], (kid, p, r)
    assert 0.0 <= r["sigma_activation"] <= 1.0, r
    expected = r["n_active"] / r["max_kernels"]
    assert abs(r["sigma_activation"] - expected) < 1e-4, (r, expected)

assert n_over == 1, d
assert n_accepted >= 4, d
assert d["n_accepted"] == n_accepted, d
assert d["n_rejected"] == 5 - n_accepted, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v239: non-deterministic" >&2; exit 1; }
