#!/usr/bin/env bash
#
# v219 σ-Create — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 8 requests × 5 candidates
#   * every SAFE winner σ_novelty ≤ τ_novelty_safe
#   * every CREATIVE winner σ_novelty ≥ min_novelty_creative
#   * refinement monotone: σ_quality_after ≥ σ_quality_before
#   * σ_surprise cross-check passes for every request
#   * impact == σ_novelty · σ_quality
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v219_create"
[ -x "$BIN" ] || { echo "v219: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v219", d
assert d["n_requests"]   == 8, d
assert d["n_candidates"] == 5, d
assert d["chain_valid"] is True, d
assert d["n_refine_monotone"]    == 8, d
assert d["n_surprise_consistent"] == 8, d

# Mode/level counts: 2 per mode, balanced SAFE/CREATIVE.
safe     = [r for r in d["requests"] if r["level"] == 0]
creative = [r for r in d["requests"] if r["level"] == 1]
assert len(safe) == 4 and len(creative) == 4, (len(safe), len(creative))
assert d["n_safe_ok"]     == len(safe), d
assert d["n_creative_ok"] == len(creative), d

for r in d["requests"]:
    assert 0.0 <= r["wn"] <= 1.0 + 1e-6, r
    assert 0.0 <= r["wq"] <= 1.0 + 1e-6, r
    assert 0.0 <= r["ws"] <= 1.0 + 1e-6, r
    assert abs(r["impact"] - r["wn"] * r["wq"]) < 1e-3, r
    assert r["wqa"] >= r["wqb"] - 1e-6, r
    if r["level"] == 0:
        assert r["wn"] <= d["tau_novelty_safe"] + 1e-6, r
    else:
        assert r["wn"] >= d["min_novelty_creative"] - 1e-6, r
        assert r["wn"] <= d["tau_novelty_creative"] + 1e-6, r
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v219: non-deterministic" >&2; exit 1; }
