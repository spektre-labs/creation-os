#!/usr/bin/env bash
#
# v204 σ-Hypothesis — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * N=10 hypotheses
#   * top-3 promoted, rest to memory
#   * impact(top-3) ≥ impact(rest)
#   * ≥ 1 REJECTED (grounding conflict) with impact==0
#   * ≥ 1 SPECULATIVE (σ_hypothesis > τ_spec) — not pruned
#   * all σ ∈ [0,1], impact ∈ [0,1]
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v204_hypothesis"
[ -x "$BIN" ] || { echo "v204: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v204", d
assert d["n"] == 10, d
assert d["n_test_queue"] == 3, d
assert d["n_to_memory"]  == 7, d
assert d["n_rejected"]    >= 1, d
assert d["n_speculative"] >= 1, d
assert d["chain_valid"]   is True, d

for h in d["hypotheses"]:
    for f in ("sh", "sg", "sn", "imp"):
        assert 0.0 <= h[f] <= 1.0 + 1e-6, (f, h)

top = [h for h in d["hypotheses"] if h["dest"] == 0]
rest = [h for h in d["hypotheses"] if h["dest"] == 1]
assert len(top) == 3 and len(rest) == 7
assert min(h["imp"] for h in top) >= max(h["imp"] for h in rest)

# At least one rejected with impact == 0.
zeroed = [h for h in d["hypotheses"] if h["status"] == 2 and h["imp"] == 0.0]
assert len(zeroed) >= 1, d["hypotheses"]

# Ranks unique 1..N.
ranks = sorted(h["rank"] for h in d["hypotheses"])
assert ranks == list(range(1, 11)), ranks
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v204: non-deterministic" >&2; exit 1; }
