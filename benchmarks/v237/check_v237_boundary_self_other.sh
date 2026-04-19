#!/usr/bin/env bash
#
# v237 σ-Boundary — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_claims == 12
#   * every zone in {SELF, OTHER, WORLD, AMBIG}
#   * n_self >= 3, n_other >= 3, n_world >= 3, n_ambig == 2
#   * n_self + n_other + n_world + n_ambig == n_claims
#   * every AMBIG row has violation == true AND the text
#     matches "\\bwe\\b" or "\\bour\\b" (case-insensitive)
#   * n_violations == 2
#   * sigma_boundary == 1 - n_agreements/n_claims (within 1e-6)
#     AND sigma_boundary in (0, 0.25)
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v237_boundary"
[ -x "$BIN" ] || { echo "v237: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json, re
d = json.loads("""$OUT""")
assert d["kernel"] == "v237", d
assert d["chain_valid"] is True, d
assert d["n_claims"] == 12, d

zones = {"SELF", "OTHER", "WORLD", "AMBIG"}
n_agree = 0
n_self = n_other = n_world = n_ambig = 0
n_violations = 0
enmeshment_pat = re.compile(r"\b(we|our)\b", re.IGNORECASE)
for c in d["claims"]:
    assert c["declared"] in zones, c
    assert c["ground_truth"] in zones, c
    if c["declared"] == "SELF":  n_self  += 1
    if c["declared"] == "OTHER": n_other += 1
    if c["declared"] == "WORLD": n_world += 1
    if c["declared"] == "AMBIG": n_ambig += 1
    if c["agreement"]: n_agree += 1
    if c["violation"]:
        assert c["declared"] == "AMBIG", c
        assert c["enmeshment"] is True, c
        assert enmeshment_pat.search(c["text"]), c
        n_violations += 1
    if c["declared"] == "AMBIG":
        assert c["violation"] is True, c

assert n_self  >= 3, d
assert n_other >= 3, d
assert n_world >= 3, d
assert n_ambig == 2, d
assert n_self + n_other + n_world + n_ambig == 12, d
assert (n_self, n_other, n_world, n_ambig) == (
    d["n_self"], d["n_other"], d["n_world"], d["n_ambig"]), d
assert n_violations == 2, d
assert d["n_violations"] == 2, d

expected = 1.0 - n_agree / 12.0
# sigma_boundary is serialised as %.4f; allow for that rounding.
assert abs(d["sigma_boundary"] - expected) < 5e-4, (d, expected)
assert 0.0 < d["sigma_boundary"] < 0.25, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v237: non-deterministic" >&2; exit 1; }
