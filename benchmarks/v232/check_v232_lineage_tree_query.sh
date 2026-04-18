#!/usr/bin/env bash
#
# v232 σ-Lineage — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 nodes, max_gen = 2, exactly one root
#   * every non-root's parent has strictly smaller gen
#   * σ_divergence ∈ [0, 1]
#   * ancestor_path walks root → node; path[last] == id
#   * n_mergeable ≥ 1 AND n_blocked ≥ 1 AND
#     n_mergeable + n_blocked == n_nodes − 1
#   * mergeable iff σ_merge ≤ τ_merge (non-root)
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v232_lineage"
[ -x "$BIN" ] || { echo "v232: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v232", d
assert d["n_nodes"] == 6, d
assert d["max_gen"] == 2, d
assert d["chain_valid"] is True, d

roots = [n for n in d["nodes"] if n["parent"] == -1]
assert len(roots) == 1, roots
by_id = {n["id"]: n for n in d["nodes"]}

for n in d["nodes"]:
    if n["parent"] >= 0:
        p = by_id[n["parent"]]
        assert p["gen"] < n["gen"], (p, n)
    assert 0.0 <= n["sigma_divergence"] <= 1.0 + 1e-6, n
    path = n["ancestor_path"]
    assert by_id[path[0]]["parent"] == -1, n
    assert path[-1] == n["id"], n
    assert n["ancestor_depth"] == n["gen"], n

tau = d["tau_merge"]
for n in d["nodes"]:
    if n["parent"] < 0:
        assert n["mergeable"] is False, n
    else:
        assert n["mergeable"] is (n["sigma_merge"] <= tau + 1e-6), n

assert d["n_mergeable"] >= 1, d
assert d["n_blocked"]   >= 1, d
assert d["n_mergeable"] + d["n_blocked"] == d["n_nodes"] - 1, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v232: non-deterministic" >&2; exit 1; }
