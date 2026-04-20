#!/usr/bin/env bash
#
# v293 σ-Hagia-Sofia — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 adoption metrics canonical (daily_users,
#     api_calls, sigma_evaluations); all
#     tracked=true
#   * 3 multi-purpose domains canonical (llm,
#     sensor, organization); σ-gate applicable on
#     every row
#   * 3 community properties canonical
#     (open_source_agpl, community_maintainable,
#     vendor_independent); all hold
#   * 3 lifecycle phases canonical
#     (active_original_purpose,
#     declining_usage, repurposed); all alive;
#     declining_usage row has warning_issued;
#     repurposed row has new_domain_found
#   * sigma_hagia in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v293_hagia_sofia"
[ -x "$BIN" ] || { echo "v293: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v293", d
assert d["chain_valid"] is True, d

M = d["metric"]
want_m = ["daily_users","api_calls","sigma_evaluations"]
assert [r["name"] for r in M] == want_m, M
for r in M:
    assert r["tracked"] is True, r
assert d["n_metric_rows_ok"]     == 3, d
assert d["metric_all_tracked_ok"] is True, d

D = d["domain"]
want_d = ["llm","sensor","organization"]
assert [r["domain"] for r in D] == want_d, D
for r in D:
    assert r["sigma_gate_applicable"] is True, r
assert d["n_domain_rows_ok"]        == 3, d
assert d["domain_all_applicable_ok"] is True, d

C = d["community"]
want_c = ["open_source_agpl","community_maintainable","vendor_independent"]
assert [r["property"] for r in C] == want_c, C
for r in C:
    assert r["holds"] is True, r
assert d["n_community_rows_ok"]    == 3, d
assert d["community_all_hold_ok"]  is True, d

L = d["lifecycle"]
want_l = [("active_original_purpose",100),
          ("declining_usage",20),
          ("repurposed",80)]
for (ph, uc), r in zip(want_l, L):
    assert r["phase"]      == ph, r
    assert r["use_count"]  == uc, r
    assert r["alive"]      is True, r
assert L[1]["warning_issued"]   is True
assert L[2]["new_domain_found"] is True
assert d["n_lifecycle_rows_ok"]      == 3, d
assert d["lifecycle_all_alive_ok"]   is True, d
assert d["lifecycle_warning_ok"]     is True, d
assert d["lifecycle_new_domain_ok"]  is True, d

assert 0.0 <= d["sigma_hagia"] <= 1.0, d
assert d["sigma_hagia"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v293: non-deterministic" >&2; exit 1; }
