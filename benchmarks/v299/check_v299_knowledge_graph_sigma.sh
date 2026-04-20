#!/usr/bin/env bash
#
# v299 σ-Knowledge-Graph — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 retrieval rows in order
#     (known_fact, partial_match, unknown);
#     sigma_retrieval strictly increasing;
#     route == FROM_KG iff sigma_retrieval <= 0.40
#     (both branches fire); exactly 2 FROM_KG and
#     exactly 1 FALLBACK_LLM
#   * 3 provenance rows in order
#     (primary_source, secondary_source, rumor_source);
#     sigma_provenance strictly increasing; trusted iff
#     sigma_provenance <= 0.50 (both branches fire);
#     every trusted row carries a non-empty source_ref
#   * 3 multi-hop chains in order (1_hop, 3_hop, 5_hop);
#     hops strictly increasing; sigma_total strictly
#     increasing; sigma_total matches
#     1 - (1 - sigma_per_hop)^hops within 1e-3;
#     warning iff sigma_total > 0.50
#     (both branches fire)
#   * 3 corpus triplets (sigma / k_eff /
#     one_equals_one); every row well_formed AND
#     queryable
#   * sigma_kg in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v299_knowledge_graph"
[ -x "$BIN" ] || { echo "v299: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v299_knowledge_graph", d
assert d["chain_valid"] is True, d

RET = d["retrieval"]
want_q = ["known_fact", "partial_match", "unknown"]
assert [r["query_kind"] for r in RET] == want_q, RET
prev = -1.0
for r in RET:
    assert r["sigma_retrieval"] > prev, RET
    prev = r["sigma_retrieval"]
TAU_KG = d["tau_kg"]
assert abs(TAU_KG - 0.40) < 1e-6, d
for r in RET:
    expected = "FROM_KG" if r["sigma_retrieval"] <= TAU_KG else "FALLBACK_LLM"
    assert r["route"] == expected, r
n_kg = sum(1 for r in RET if r["route"] == "FROM_KG")
n_fb = sum(1 for r in RET if r["route"] == "FALLBACK_LLM")
assert n_kg == 2 and n_fb == 1, RET
assert d["ret_rows_ok"]           == 3,    d
assert d["ret_order_ok"]          is True, d
assert d["ret_gate_polarity_ok"]  is True, d
assert d["ret_fromkg_count_ok"]   is True, d

PROV = d["provenance"]
want_s = ["primary_source", "secondary_source", "rumor_source"]
assert [r["source"] for r in PROV] == want_s, PROV
prev = -1.0
for r in PROV:
    assert r["sigma_provenance"] > prev, PROV
    prev = r["sigma_provenance"]
TAU_P = d["tau_prov"]
assert abs(TAU_P - 0.50) < 1e-6, d
for r in PROV:
    expected_trusted = r["sigma_provenance"] <= TAU_P
    assert r["trusted"] is expected_trusted, r
    if r["trusted"]:
        assert len(r["source_ref"]) > 0, r
n_t = sum(1 for r in PROV if r["trusted"])
n_u = sum(1 for r in PROV if not r["trusted"])
assert n_t >= 1 and n_u >= 1, PROV
assert d["prov_rows_ok"]     == 3,    d
assert d["prov_order_ok"]    is True, d
assert d["prov_polarity_ok"] is True, d
assert d["prov_source_ok"]   is True, d

HOP = d["multi_hop"]
want_h = ["1_hop", "3_hop", "5_hop"]
assert [r["label"] for r in HOP] == want_h, HOP
want_n = [1, 3, 5]
assert [r["hops"] for r in HOP] == want_n, HOP
prev_h, prev_s = -1, -1.0
for r in HOP:
    assert r["hops"] > prev_h, HOP
    assert r["sigma_total"] > prev_s, HOP
    prev_h, prev_s = r["hops"], r["sigma_total"]
    expected = 1.0 - (1.0 - r["sigma_per_hop"]) ** r["hops"]
    assert abs(r["sigma_total"] - expected) < 1e-3, r
TAU_W = d["tau_warning"]
assert abs(TAU_W - 0.50) < 1e-6, d
for r in HOP:
    assert r["warning"] is (r["sigma_total"] > TAU_W), r
n_w = sum(1 for r in HOP if r["warning"])
assert n_w >= 1 and n_w < len(HOP), HOP
assert d["hop_rows_ok"]              == 3,    d
assert d["hop_order_ok"]             is True, d
assert d["hop_formula_ok"]           is True, d
assert d["hop_warning_polarity_ok"]  is True, d

COR = d["corpus"]
want_sub = ["sigma", "k_eff", "one_equals_one"]
want_rel = ["IS_SNR_OF", "DEPENDS_ON", "EXPRESSES"]
want_obj = ["noise_signal_ratio", "sigma", "self_consistency"]
assert [r["subject"]  for r in COR] == want_sub, COR
assert [r["relation"] for r in COR] == want_rel, COR
assert [r["object"]   for r in COR] == want_obj, COR
for r in COR:
    assert r["well_formed"] is True, r
    assert r["queryable"]   is True, r
assert d["corpus_rows_ok"]         == 3,    d
assert d["corpus_wellformed_ok"]   is True, d
assert d["corpus_queryable_ok"]    is True, d

assert 0.0 <= d["sigma_kg"] <= 1.0, d
assert d["sigma_kg"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v299: non-deterministic" >&2; exit 1; }
