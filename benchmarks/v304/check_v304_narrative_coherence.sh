#!/usr/bin/env bash
#
# v304 σ-Narrative — merge-gate check.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v304_narrative"
[ -x "$BIN" ] || { echo "v304: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v304_narrative", d
assert d["chain_valid"] is True, d

ST = d["stories"]
want = ["coherent", "minor_tension", "contradictory"]
assert [r["label"] for r in ST] == want, ST
TAU_S = d["tau_story"]
assert abs(TAU_S - 0.40) < 1e-6, d
prev = -1.0
for r in ST:
    assert r["sigma_narrative"] > prev, ST
    prev = r["sigma_narrative"]
    expected = "COHERENT" if r["sigma_narrative"] <= TAU_S else "CONTRADICTORY"
    assert r["verdict"] == expected, r
n_co = sum(1 for r in ST if r["verdict"] == "COHERENT")
n_ct = sum(1 for r in ST if r["verdict"] == "CONTRADICTORY")
assert n_co == 2 and n_ct == 1, ST
assert d["story_rows_ok"]     == 3,    d
assert d["story_order_ok"]    is True, d
assert d["story_polarity_ok"] is True, d
assert d["story_count_ok"]    is True, d

A = d["arguments"]
want_a = ["valid_modus_ponens", "weak_induction", "affirming_consequent"]
assert [r["step"] for r in A] == want_a, A
TAU_A = d["tau_arg"]
prev = -1.0
for r in A:
    assert r["sigma_arg"] > prev, A
    prev = r["sigma_arg"]
    assert r["valid"] is (r["sigma_arg"] <= TAU_A), r
n_v = sum(1 for r in A if r["valid"])
n_i = sum(1 for r in A if not r["valid"])
assert n_v >= 1 and n_i >= 1, A
assert d["arg_rows_ok"]     == 3,    d
assert d["arg_order_ok"]    is True, d
assert d["arg_polarity_ok"] is True, d

P = d["propaganda"]
want_p = ["neutral_report", "persuasive_essay", "manipulative_ad"]
assert [r["text"] for r in P] == want_p, P
TAU_P = d["tau_prop"]
for r in P:
    expected = r["emotion_load"] * r["logic_sigma"]
    assert abs(r["propaganda_score"] - expected) < 1e-3, r
    assert r["flagged"] is (r["propaganda_score"] > TAU_P), r
n_f = sum(1 for r in P if r["flagged"])
n_o = sum(1 for r in P if not r["flagged"])
assert n_f >= 1 and n_o >= 1, P
assert d["prop_rows_ok"]      == 3,    d
assert d["prop_formula_ok"]   is True, d
assert d["prop_polarity_ok"]  is True, d

F = d["self_stories"]
want_f = ["aligned_self", "slight_misalignment", "denial"]
assert [r["story"] for r in F] == want_f, F
TAU_F = d["tau_self"]
prev = -1.0
for r in F:
    assert r["sigma_self"] > prev, F
    prev = r["sigma_self"]
    assert r["matches_facts"] is (r["sigma_self"] <= TAU_F), r
n_m = sum(1 for r in F if r["matches_facts"])
n_n = sum(1 for r in F if not r["matches_facts"])
assert n_m >= 1 and n_n >= 1, F
assert d["self_rows_ok"]      == 3,    d
assert d["self_order_ok"]     is True, d
assert d["self_polarity_ok"]  is True, d

assert 0.0 <= d["sigma_narr"] <= 1.0, d
assert d["sigma_narr"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v304: non-deterministic" >&2; exit 1; }
