#!/usr/bin/env bash
#
# v199 σ-Law — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 jurisdictions each with ≥ 3 norms
#   * every resolution verdict ∈ {PERMITTED, PROHIBITED, REVIEW}
#   * at least one higher-priority win
#   * at least one waiver applied (PROHIBITED → PERMITTED)
#   * at least one REVIEW escalation
#   * same-priority contradictory norms ⇒ σ_law == 1.0 + REVIEW
#   * governance-chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v199_law"
[ -x "$BIN" ] || { echo "v199: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]    == "v199", d
assert d["n_norms"]   == 18,      d
assert d["n_queries"] == 16,      d
assert all(c >= 3 for c in d["per_jur_norm_count"]), d
assert d["n_higher_priority_wins"] >= 1, d
assert d["n_waivers_applied"]      >= 1, d
assert d["n_review_escalations"]   >= 1, d
assert d["chain_valid"]            is True, d

for r in d["resolutions"]:
    assert r["verdict"] in (0, 1, 2), r
    assert 0.0 <= r["sigma_law"] <= 1.0 + 1e-6, r
    if r["verdict"] == 2:
        assert r["review"] is True, r

# Find the SCSL same-priority topic-4 query: σ_law must be 1.0.
topic4 = [r for r in d["resolutions"] if r["jur"] == 0 and r["topic"] == 4]
assert topic4, "no topic-4 query"
for r in topic4:
    assert r["verdict"] == 2, ("topic-4 same-prio conflict → REVIEW", r)
    assert abs(r["sigma_law"] - 1.0) < 1e-6, r
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v199: non-deterministic output" >&2
    exit 1
fi
