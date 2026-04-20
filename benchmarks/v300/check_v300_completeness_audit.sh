#!/usr/bin/env bash
#
# v300 σ-Complete — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * kernels_total == 300
#   * 15 cognitive categories (v243 list) in canonical
#     order; every category covered=true with a
#     representative_kernel in [6, 300]
#   * dependency graph with 3 buckets
#     (core_critical, supporting, removable_duplicate);
#     counts sum to 300; removable_duplicate == 0;
#     core_critical == 7
#   * 4 repo-level 1=1 claims (zero_deps / sigma_gated /
#     deterministic / monotonic_clock); declared ==
#     realized on every row; sigma_pair == 0;
#     sigma_repo == 0 < 0.10
#   * 7 pyramid invariants in canonical order (v287
#     granite / v288 oculus / v289 ruin_value / v290
#     dougong / v293 hagia_sofia / v297 clock / v298
#     rosetta); every row invariant_holds=true;
#     architecture_survives_100yr=true
#   * sigma_complete in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v300_complete"
[ -x "$BIN" ] || { echo "v300: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v300_complete", d
assert d["chain_valid"] is True, d
assert d["kernels_total"] == 300, d
assert abs(d["tau_repo"] - 0.10) < 1e-6, d

COG = d["cognitive"]
want_cat = [
    "perception", "memory", "reasoning",
    "planning", "learning", "language",
    "action", "metacognition", "emotion",
    "social", "ethics", "creativity",
    "self_model", "embodiment", "consciousness",
]
assert [r["category"] for r in COG] == want_cat, COG
assert len(COG) == 15, COG
for r in COG:
    k = r["representative_kernel"]
    assert 6 <= k <= 300, r
    assert r["covered"] is True, r
assert d["cog_rows_ok"]    == 15,   d
assert d["cog_covered_ok"] is True, d

DEP = d["dependency"]
want_bucket = ["core_critical", "supporting", "removable_duplicate"]
assert [r["bucket"] for r in DEP] == want_bucket, DEP
total = sum(r["count"] for r in DEP)
assert total == 300, DEP
assert DEP[0]["count"] == 7, DEP
assert DEP[2]["count"] == 0, DEP
assert d["dep_sum_ok"]              is True, d
assert d["dep_removable_zero_ok"]   is True, d
assert d["dep_critical_count_ok"]   is True, d

REPO = d["one_equals_one"]
want_claim = ["zero_deps", "sigma_gated",
              "deterministic", "monotonic_clock"]
assert [r["claim"] for r in REPO] == want_claim, REPO
assert len(REPO) == 4, REPO
for r in REPO:
    assert r["declared_in_readme"] == r["realized_in_code"], r
    assert abs(r["sigma_pair"]) < 1e-6, r
assert d["repo_rows_ok"]                    == 4,    d
assert d["repo_declared_eq_realized_ok"]    is True, d
assert d["sigma_repo"] < 0.10, d
assert abs(d["sigma_repo"]) < 1e-6, d
assert d["repo_sigma_under_threshold_ok"]   is True, d

PYR = d["pyramid"]
want_pyr = [
    "v287_granite", "v288_oculus",
    "v289_ruin_value", "v290_dougong",
    "v293_hagia_sofia", "v297_clock",
    "v298_rosetta",
]
assert [r["kernel_label"] for r in PYR] == want_pyr, PYR
assert len(PYR) == 7, PYR
for r in PYR:
    assert r["invariant_holds"] is True, r
assert d["pyr_rows_ok"]                  == 7,    d
assert d["pyr_all_hold_ok"]              is True, d
assert d["architecture_survives_100yr"]  is True, d

assert 0.0 <= d["sigma_complete"] <= 1.0, d
assert d["sigma_complete"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v300: non-deterministic" >&2; exit 1; }
