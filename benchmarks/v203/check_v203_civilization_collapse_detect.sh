#!/usr/bin/env bash
#
# v203 σ-Civilization — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 institutions (≥ 1 SCSL, ≥ 1 CLOSED, ≥ 1 PRIVATE)
#   * ≥ 1 collapse + ≥ 1 recovery detected
#   * stable > recovered > permanently-collapsed continuity
#   * ≥ 1 inter-layer contradiction escalated to REVIEW
#   * SCSL public-ratio strictly higher than CLOSED
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v203_civilization"
[ -x "$BIN" ] || { echo "v203: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"] == "v203", d
assert d["n_institutions"] == 6, d
assert d["n_ticks"]        == 12, d
assert d["n_collapses_total"]  >= 1, d
assert d["n_recoveries_total"] >= 1, d
assert d["n_contradictions"]   >= 1, d
assert d["chain_valid"] is True, d
assert d["public_ratio_scsl"] > d["public_ratio_closed"] + 0.10, d

# License diversity.
licenses = {ins["license"] for ins in d["institutions"]}
assert {0, 1, 2}.issubset(licenses), licenses

# Continuity ordering (id 0 stable, id 1 recovered, id 3 collapsed).
c0 = d["institutions"][0]["continuity"]
c1 = d["institutions"][1]["continuity"]
c3 = d["institutions"][3]["continuity"]
assert c0 > c1 > c3, (c0, c1, c3)
assert d["institutions"][3]["permanent_collapse"] is True

# At least one escalated contradiction.
escalated = [c for c in d["contras"] if c["escalated"]]
assert len(escalated) >= 1, d["contras"]
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v203: non-deterministic output" >&2
    exit 1
fi
