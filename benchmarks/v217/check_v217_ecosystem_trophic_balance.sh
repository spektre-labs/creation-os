#!/usr/bin/env bash
#
# v217 σ-Ecosystem — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 trophic levels, populations sum to pop_total
#   * no level share > τ_dom (0.40) — no dominance
#   * ≥ 3 symbiotic pairs, every σ_pair ∈ [0, 1]
#   * σ_ecosystem ∈ [0, 1], k_eff ∈ [0, 1]
#   * k_eff > τ_healthy (0.55) — v0 is a healthy snapshot
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v217_ecosystem"
[ -x "$BIN" ] || { echo "v217: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v217", d
assert len(d["levels"]) == 4, d
assert sum(l["pop"] for l in d["levels"]) == d["pop_total"], d
for l in d["levels"]:
    assert l["share"] <= d["tau_dom"] + 1e-6, l

assert len(d["pairs"]) >= 3, d
for p in d["pairs"]:
    assert 0.0 <= p["s"] <= 1.0 + 1e-6, p

for f in ("sigma_dominance","sigma_balance","sigma_symbiosis",
          "sigma_ecosystem","k_eff"):
    assert 0.0 <= d[f] <= 1.0 + 1e-6, (f, d[f])

assert d["k_eff"] > d["tau_healthy"], d
assert d["chain_valid"] is True, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v217: non-deterministic" >&2; exit 1; }
