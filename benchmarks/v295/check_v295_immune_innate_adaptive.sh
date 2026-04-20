#!/usr/bin/env bash
#
# v295 σ-Immune — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 innate patterns (sql_injection/prompt_injection/
#     obvious_malware); σ_raw ≥ 0.70; all blocked;
#     none require_training; all response_tier=INSTANT
#   * 3 adaptive rows (novel_attack_first_seen/
#     same_attack_second_seen/related_variant_seen);
#     all learned; exactly 1 faster_on_repeat (the
#     second encounter); exactly 1 cross_recognized
#     (the variant)
#   * 3 memory rows (pattern_A_first_logged/
#     pattern_A_reencountered/pattern_B_new_logged);
#     recognised iff tier==FAST; exactly 1 recognised
#     (the re-encounter)
#   * 3 autoimmune scenarios (tau_too_tight/
#     tau_balanced/tau_too_loose); τ strictly
#     increasing; 3 DISTINCT verdicts (AUTOIMMUNE,
#     HEALTHY, IMMUNODEFICIENT); HEALTHY iff
#     τ ∈ [0.10, 0.60] AND fpr ≤ 0.10
#   * sigma_immune in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v295_immune"
[ -x "$BIN" ] || { echo "v295: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v295", d
assert d["chain_valid"] is True, d

IN = d["innate"]
want_i = ["sql_injection","prompt_injection","obvious_malware"]
assert [r["pattern"] for r in IN] == want_i, IN
for r in IN:
    assert r["sigma_raw"] >= 0.70 - 1e-6, r
    assert r["blocked"]             is True,  r
    assert r["requires_training"]   is False, r
    assert r["response_tier"]       == "INSTANT", r
assert d["n_innate_rows_ok"]       == 3, d
assert d["innate_all_blocked_ok"]  is True, d
assert d["innate_all_instant_ok"]  is True, d

AD = d["adapt"]
want_a = ["novel_attack_first_seen",
          "same_attack_second_seen",
          "related_variant_seen"]
assert [r["row"] for r in AD] == want_a, AD
for r in AD:
    assert r["learned"] is True, r
n_fast = sum(1 for r in AD if r["faster_on_repeat"])
n_cross = sum(1 for r in AD if r["cross_recognized"])
assert n_fast  == 1, AD
assert n_cross == 1, AD
assert AD[1]["faster_on_repeat"] is True, AD[1]
assert AD[2]["cross_recognized"] is True, AD[2]
assert d["n_adaptive_rows_ok"]       == 3, d
assert d["adaptive_all_learned_ok"]  is True, d
assert d["adaptive_faster_ok"]       is True, d
assert d["adaptive_cross_ok"]        is True, d

ME = d["memory"]
want_m = ["pattern_A_first_logged",
          "pattern_A_reencountered",
          "pattern_B_new_logged"]
assert [r["entry"] for r in ME] == want_m, ME
for r in ME:
    fast = (r["tier"] == "FAST")
    assert r["recognised"] is fast, r
assert sum(1 for r in ME if r["recognised"]) == 1, ME
assert d["n_memory_rows_ok"]            == 3, d
assert d["memory_recog_polarity_ok"]    is True, d
assert d["memory_count_ok"]             is True, d

AP = d["autoprev"]
want_s = ["tau_too_tight","tau_balanced","tau_too_loose"]
assert [r["scenario"] for r in AP] == want_s, AP
prev = -1.0
verdicts = set()
for r in AP:
    assert r["tau"] > prev, r
    prev = r["tau"]
    verdicts.add(r["verdict"])
    in_range = (0.10 - 1e-6 <= r["tau"] <= 0.60 + 1e-6 and
                r["false_positive_rate"] <= 0.10 + 1e-6)
    if r["verdict"] == "HEALTHY":
        assert in_range, r
    else:
        assert not in_range, r
assert verdicts == {"AUTOIMMUNE","HEALTHY","IMMUNODEFICIENT"}, verdicts
assert d["n_auto_rows_ok"]              == 3, d
assert d["auto_order_ok"]               is True, d
assert d["auto_verdict_ok"]             is True, d
assert d["auto_healthy_range_ok"]       is True, d

assert 0.0 <= d["sigma_immune"] <= 1.0, d
assert d["sigma_immune"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v295: non-deterministic" >&2; exit 1; }
