#!/usr/bin/env bash
#
# v274 σ-Industrial — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 process params canonical (temperature, pressure,
#     speed, material); σ_process == max(σ_param);
#     action ∈ {CONTINUE, HALT} with CONTINUE iff
#     σ_process <= 0.40; process_aggregation_ok AND
#     process_action_ok; fixture drives HALT branch
#   * 4 supply links canonical (supplier, factory,
#     distribution, customer); backup_activated iff
#     σ_link > 0.45; >=1 backup AND >=1 no-backup
#   * 3 quality rows; action ∈ {SKIP_MANUAL,
#     REQUIRE_MANUAL} with SKIP iff σ_quality <= 0.25;
#     both branches
#   * 3 OEE shift fixtures; oee == a*p*q (within 1e-5);
#     trustworthy iff σ_oee <= 0.20; both branches
#   * sigma_industrial in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v274_industrial"
[ -x "$BIN" ] || { echo "v274: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v274", d
assert d["chain_valid"] is True, d
assert abs(d["tau_process"] - 0.40) < 1e-6, d
assert abs(d["tau_backup"]  - 0.45) < 1e-6, d
assert abs(d["tau_quality"] - 0.25) < 1e-6, d
assert abs(d["tau_oee"]     - 0.20) < 1e-6, d

P = d["params"]
want_p = ["temperature","pressure","speed","material"]
assert [p["param"] for p in P] == want_p, P
mx = max(p["sigma_param"] for p in P)
assert abs(d["sigma_process"] - mx) < 1e-5, d
assert d["process_aggregation_ok"] is True, d
exp_pa = "CONTINUE" if d["sigma_process"] <= d["tau_process"] else "HALT"
assert d["process_action"] == exp_pa, d
assert d["process_action"] == "HALT", d
assert d["process_action_ok"] is True, d
assert d["n_process_params_ok"] == 4, d

S = d["supply"]
want_s = ["supplier","factory","distribution","customer"]
assert [y["link"] for y in S] == want_s, S
n_b = n_ok = 0
for y in S:
    exp = y["sigma_link"] > d["tau_backup"]
    assert y["backup_activated"] == exp, y
    if y["backup_activated"]: n_b  += 1
    else:                       n_ok += 1
assert n_b >= 1 and n_ok >= 1, (n_b, n_ok)
assert d["n_supply_rows_ok"]  == 4, d
assert d["n_supply_backup"]   == n_b, d
assert d["n_supply_ok_link"]  == n_ok, d

Q = d["quality"]
assert len(Q) == 3
n_s = n_r = 0
for q in Q:
    exp = "SKIP_MANUAL" if q["sigma_quality"] <= d["tau_quality"] else "REQUIRE_MANUAL"
    assert q["action"] == exp, q
    if q["action"] == "SKIP_MANUAL":    n_s += 1
    if q["action"] == "REQUIRE_MANUAL": n_r += 1
assert n_s >= 1 and n_r >= 1, (n_s, n_r)
assert d["n_quality_rows_ok"]   == 3, d
assert d["n_quality_skip"]      == n_s, d
assert d["n_quality_require"]   == n_r, d

O = d["oee"]
assert len(O) == 3
n_t = n_u = 0
for o in O:
    expected = o["availability"] * o["performance"] * o["quality"]
    assert abs(o["oee"] - expected) < 1e-4, o
    exp = o["sigma_oee"] <= d["tau_oee"]
    assert o["trustworthy"] == exp, o
    if o["trustworthy"]: n_t += 1
    else:                  n_u += 1
assert n_t >= 1 and n_u >= 1, (n_t, n_u)
assert d["n_oee_rows_ok"]      == 3, d
assert d["n_oee_formula_ok"]   == 3, d
assert d["n_oee_trust"]        == n_t, d
assert d["n_oee_untrust"]      == n_u, d

assert 0.0 <= d["sigma_industrial"] <= 1.0, d
assert d["sigma_industrial"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v274: non-deterministic" >&2; exit 1; }
