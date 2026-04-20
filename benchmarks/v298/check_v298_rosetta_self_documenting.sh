#!/usr/bin/env bash
#
# v298 σ-Rosetta — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 σ emissions; 3 DISTINCT domains (LLM, SENSOR,
#     ORG) in order; reason_present=true on every row;
#     reason length ≥ 20 characters
#   * 3 language bindings (C REFERENCE / Python
#     ADOPTION / Rust SAFETY); 3 DISTINCT roles; every
#     row maintained=true AND semantic_match_to_c=true
#   * 3 log formats (binary/csv/json); all
#     machine_readable=true; exactly 2 with
#     human_readable_forever=true (csv, json);
#     exactly 1 with human_readable_forever=false
#     (binary)
#   * 3 math invariants (sigma_definition/
#     pythagoras_2500_yr/arithmetic_invariant);
#     formal_expression_present=true AND ages_out=false
#     on every row
#   * sigma_rosetta in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v298_rosetta"
[ -x "$BIN" ] || { echo "v298: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v298", d
assert d["chain_valid"] is True, d

EM = d["emit"]
want_d = ["LLM","SENSOR","ORG"]
assert [r["domain"] for r in EM] == want_d, EM
for r in EM:
    assert r["reason_present"] is True, r
    assert len(r["reason"]) >= 20, r
assert d["n_emit_rows_ok"]            == 3, d
assert d["emit_domain_distinct_ok"]   is True, d
assert d["emit_reason_ok"]            is True, d

SP = d["spec"]
want_lang = ["C","Python","Rust"]
want_role = ["REFERENCE","ADOPTION","SAFETY"]
assert [r["language"] for r in SP] == want_lang, SP
assert [r["role"]     for r in SP] == want_role, SP
for r in SP:
    assert r["maintained"]          is True, r
    assert r["semantic_match_to_c"] is True, r
assert d["n_spec_rows_ok"]           == 3, d
assert d["spec_role_distinct_ok"]    is True, d
assert d["spec_maintained_ok"]       is True, d
assert d["spec_match_ok"]            is True, d

FM = d["fmt"]
want_f = ["binary","csv","json"]
assert [r["format"] for r in FM] == want_f, FM
for r in FM:
    assert r["machine_readable"] is True, r
n_human = sum(1 for r in FM if r["human_readable_forever"])
assert n_human == 2, FM
assert FM[0]["human_readable_forever"] is False, FM[0]
assert FM[1]["human_readable_forever"] is True,  FM[1]
assert FM[2]["human_readable_forever"] is True,  FM[2]
assert d["n_fmt_rows_ok"]         == 3, d
assert d["fmt_machine_ok"]        is True, d
assert d["fmt_human_count_ok"]    is True, d

MA = d["math"]
want_m = ["sigma_definition","pythagoras_2500_yr","arithmetic_invariant"]
assert [r["name"] for r in MA] == want_m, MA
for r in MA:
    assert r["formal_expression_present"] is True,  r
    assert r["ages_out"]                  is False, r
    assert len(r["formula"]) > 0, r
assert d["n_math_rows_ok"]        == 3, d
assert d["math_expression_ok"]    is True, d
assert d["math_ages_out_ok"]      is True, d

assert 0.0 <= d["sigma_rosetta"] <= 1.0, d
assert d["sigma_rosetta"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v298: non-deterministic" >&2; exit 1; }
