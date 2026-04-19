#!/usr/bin/env bash
#
# v243 σ-Complete — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 15 categories in canonical order
#   * every category has >=1 kernel id, tier in {M,P},
#     sigma_category in [0,1], covered==true,
#     declared_eq_realized==true
#   * n_covered == 15; sigma_completeness in [0,1] AND == 0.0
#   * one_equals_one == true; cognitive_complete == true
#   * n_m_tier + n_p_tier == 15
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v243_complete"
[ -x "$BIN" ] || { echo "v243: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v243", d
assert d["chain_valid"] is True, d
assert d["n_categories"] == 15, d

wanted = ["PERCEPTION","MEMORY","REASONING","PLANNING","ACTION",
          "LEARNING","REFLECTION","IDENTITY","MORALITY","SOCIALITY",
          "CREATIVITY","SCIENCE","SAFETY","CONTINUITY","SOVEREIGNTY"]
names = [c["name"] for c in d["categories"]]
assert names == wanted, names

for c in d["categories"]:
    assert c["n_kernels"] >= 1, c
    assert c["tier"] in ("M", "P"), c
    assert 0.0 <= c["sigma_category"] <= 1.0, c
    assert c["covered"] is True, c
    assert c["declared_eq_realized"] is True, c
    assert len(c["kernels"]) == c["n_kernels"], c

assert d["n_covered"] == 15, d
assert d["n_m_tier"] + d["n_p_tier"] == 15, d
assert 0.0 <= d["sigma_completeness"] <= 1.0, d
assert d["sigma_completeness"] < 1e-6, d
assert d["one_equals_one"]     is True, d
assert d["cognitive_complete"] is True, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v243: non-deterministic" >&2; exit 1; }
