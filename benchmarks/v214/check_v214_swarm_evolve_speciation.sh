#!/usr/bin/env bash
#
# v214 σ-Swarm-Evolve — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 10 generations; population constant at 12 every gen
#   * final_established_species ≥ 3 (speciation into ≥ 3 niches)
#   * max_lineage_span ≥ 5 (at least one lineage lives half the run)
#   * total_retired ≥ 8 (lifecycle actually runs)
#   * σ_emergent monotone non-increasing across generations
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v214_swarm_evolve"
[ -x "$BIN" ] || { echo "v214: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v214", d
assert d["n_gens"] == 10, d
assert d["pop_size"] == 12, d
assert d["final_established_species"] >= 3, d
assert d["max_lineage_span"] >= 5, d
assert d["total_retired"] >= 8, d
assert d["chain_valid"] is True, d

gens = d["gens"]
assert len(gens) == 10, gens
for gr in gens:
    assert gr["alive"] == 12, gr
# Monotone non-increasing σ_emergent.
for a, b in zip(gens, gens[1:]):
    assert b["se"] <= a["se"] + 1e-6, (a, b)
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v214: non-deterministic" >&2; exit 1; }
