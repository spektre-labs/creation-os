#!/usr/bin/env bash
#
# v213 σ-Trust-Chain — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 7 links in canonical root→leaf order:
#     v138 → v183 → v211 → v191 → v181 → v210 → v212
#   * every link: proof_valid + audit_intact +
#     runtime_active + σ_link ∈ [0, 0.05]
#   * n_valid == 7 and broken_at_link == 0
#   * trust_score > τ_trust (0.85)
#   * chain valid + byte-deterministic (external
#     attestation reproducible)
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v213_trust_chain"
[ -x "$BIN" ] || { echo "v213: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v213", d
assert d["n"] == 7, d
assert d["n_valid"] == 7, d
assert d["broken_at_link"] == 0, d
assert d["trust_score"] > d["tau_trust"], d
assert d["chain_valid"] is True, d

expected = ["v138","v183","v211","v191","v181","v210","v212"]
srcs = [l["source"] for l in d["links"]]
assert srcs == expected, srcs
for l in d["links"]:
    assert l["proof_valid"]    is True, l
    assert l["audit_intact"]   is True, l
    assert l["runtime_active"] is True, l
    assert 0.0 <= l["s"] <= d["max_sigma_link"] + 1e-6, l
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v213: non-deterministic" >&2; exit 1; }
