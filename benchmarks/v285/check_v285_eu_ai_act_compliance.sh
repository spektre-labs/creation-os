#!/usr/bin/env bash
#
# v285 σ-EU-AI-Act — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 Art 15 rows canonical (robustness, accuracy,
#     cybersecurity); sigma_mapped AND audit_trail
#     on EVERY row
#   * 3 Art 52 rows canonical (training_docs,
#     feedback_collection, qa_process);
#     required_by_eu AND sigma_simplifies on EVERY
#     row
#   * 4 risk rows canonical (low, medium, high,
#     critical) with σ cascade τ=0.20/0.50/0.80;
#     sigma_gate on EVERY tier; controls_count
#     strictly monotonic 1→2→3→4
#   * 3 license rows (scsl LEGAL, eu_ai_act
#     REGULATORY, sigma_gate TECHNICAL) with 3
#     DISTINCT layers; every row enabled AND
#     composable
#   * sigma_euact in [0,1] AND == 0.0
#   * deterministic JSON across re-invocations
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v285_eu_ai_act"
[ -x "$BIN" ] || { echo "v285: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v285", d
assert d["chain_valid"] is True, d
assert abs(d["tau_low"]    - 0.20) < 1e-6, d
assert abs(d["tau_medium"] - 0.50) < 1e-6, d
assert abs(d["tau_high"]   - 0.80) < 1e-6, d

A = d["art15"]
assert [a["name"] for a in A] == ["robustness","accuracy","cybersecurity"], A
for a in A:
    assert a["sigma_mapped"] is True, a
    assert a["audit_trail"]  is True, a
assert d["n_art15_rows_ok"]        == 3, d
assert d["art15_all_mapped_ok"]    is True, d
assert d["art15_all_audit_ok"]     is True, d

B = d["art52"]
assert [a["name"] for a in B] == ["training_docs","feedback_collection","qa_process"], B
for a in B:
    assert a["required_by_eu"]   is True, a
    assert a["sigma_simplifies"] is True, a
assert d["n_art52_rows_ok"]           == 3, d
assert d["art52_all_required_ok"]     is True, d
assert d["art52_all_simplifies_ok"]   is True, d

R = d["risk"]
want = [("low",1), ("medium",2), ("high",3), ("critical",4)]
for (lbl, cnt), row in zip(want, R):
    assert row["label"] == lbl, row
    assert row["sigma_gate"] is True, row
    assert row["controls_count"] == cnt, row
counts = [r["controls_count"] for r in R]
assert counts == [1,2,3,4], counts
assert all(counts[i] > counts[i-1] for i in range(1, len(counts))), counts
assert R[-1]["sigma_gate"] and R[-1]["hitl_required"] and \
       R[-1]["audit_trail"] and R[-1]["redundancy_required"], R[-1]
assert d["n_risk_rows_ok"]      == 4, d
assert d["risk_all_sigma_ok"]   is True, d
assert d["risk_monotone_ok"]    is True, d

L = d["license"]
want_l = [("scsl","LEGAL"), ("eu_ai_act","REGULATORY"),
          ("sigma_gate","TECHNICAL")]
for (n, ly), row in zip(want_l, L):
    assert row["name"]  == n,  row
    assert row["layer"] == ly, row
    assert row["enabled"]    is True, row
    assert row["composable"] is True, row
assert len({l["layer"] for l in L}) == 3, L
assert d["n_license_rows_ok"]             == 3, d
assert d["license_layers_distinct_ok"]    is True, d
assert d["license_all_enabled_ok"]        is True, d
assert d["license_all_composable_ok"]     is True, d

assert 0.0 <= d["sigma_euact"] <= 1.0, d
assert d["sigma_euact"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v285: non-deterministic" >&2; exit 1; }
