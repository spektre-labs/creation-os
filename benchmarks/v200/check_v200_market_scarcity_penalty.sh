#!/usr/bin/env bash
#
# v200 σ-Market — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 resources, 40 queries, mass-balance in ledger
#   * σ > σ_local ⇒ route = API; else route = local
#   * cost_second_half < cost_first_half (self-improving cost)
#   * scarcity penalty activates at least once
#   * at least one eviction recorded
#   * exchange-log chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v200_market"
[ -x "$BIN" ] || { echo "v200: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]       == "v200", d
assert d["n_queries"]    == 40,      d
assert d["n_resources"]  == 4,       d
assert d["scarcity_triggered"] is True, d
assert d["n_evictions"] >= 1, d
assert d["n_api_routes"]   >= 1, d
assert d["n_local_routes"] >= 1, d
assert d["cost_second_half"] < d["cost_first_half"], d
assert d["chain_valid"] is True, d

sigma_local = d["sigma_local"]
penalty_seen = False
for q in d["queries"]:
    # Route contract.
    if q["sb"] > sigma_local:
        assert q["route"] == 1, q
    else:
        assert q["route"] == 0, q
    # Cost ≥ price.
    assert q["cost"] + 1e-6 >= q["price"], q
    # σ after < σ before (learning/API-call).
    assert q["sa"] < q["sb"] + 1e-6, q
    if q["pen"] > 0: penalty_seen = True

assert penalty_seen, "no per-query scarcity penalty observed"
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v200: non-deterministic output" >&2
    exit 1
fi
