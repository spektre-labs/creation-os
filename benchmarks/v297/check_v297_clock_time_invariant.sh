#!/usr/bin/env bash
#
# v297 σ-Clock — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 expiry rows (hardcoded_date/valid_until_2030/
#     api_version_expiry); all forbidden AND all
#     absent (present_in_kernel=false)
#   * 3 time sources (CLOCK_MONOTONIC/CLOCK_REALTIME/
#     wallclock_local); exactly 1 ALLOW with
#     in_kernel=true (CLOCK_MONOTONIC); exactly 2
#     FORBID with in_kernel=false (wallclock sources)
#   * 3 log properties (relative_sequence/
#     unix_epoch_absent/y2038_safe); all holds=true
#   * 3 protocol properties (no_version_field_on_struct/
#     old_reader_ignores_new_fields/
#     append_only_field_semantics); all holds=true
#   * sigma_clock in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v297_clock"
[ -x "$BIN" ] || { echo "v297: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v297", d
assert d["chain_valid"] is True, d

EX = d["expiry"]
want_e = ["hardcoded_date","valid_until_2030","api_version_expiry"]
assert [r["field"] for r in EX] == want_e, EX
for r in EX:
    assert r["present_in_kernel"] is False, r
    assert r["forbidden"]         is True,  r
assert d["n_expiry_rows_ok"]         == 3, d
assert d["expiry_all_forbidden_ok"]  is True, d
assert d["expiry_all_absent_ok"]     is True, d

TI = d["time"]
want_t = ["CLOCK_MONOTONIC","CLOCK_REALTIME","wallclock_local"]
assert [r["source"] for r in TI] == want_t, TI
n_allow  = sum(1 for r in TI if r["verdict"] == "ALLOW"  and r["in_kernel"])
n_forbid = sum(1 for r in TI if r["verdict"] == "FORBID" and not r["in_kernel"])
assert n_allow  == 1, TI
assert n_forbid == 2, TI
assert TI[0]["verdict"] == "ALLOW"  and TI[0]["in_kernel"] is True
assert TI[1]["verdict"] == "FORBID" and TI[1]["in_kernel"] is False
assert TI[2]["verdict"] == "FORBID" and TI[2]["in_kernel"] is False
assert d["n_time_rows_ok"]        == 3, d
assert d["time_allow_count_ok"]   is True, d
assert d["time_forbid_count_ok"]  is True, d

LO = d["log"]
want_l = ["relative_sequence","unix_epoch_absent","y2038_safe"]
assert [r["property"] for r in LO] == want_l, LO
for r in LO:
    assert r["holds"] is True, r
assert d["n_log_rows_ok"]     == 3, d
assert d["log_all_hold_ok"]   is True, d

PR = d["proto"]
want_p = ["no_version_field_on_struct",
          "old_reader_ignores_new_fields",
          "append_only_field_semantics"]
assert [r["property"] for r in PR] == want_p, PR
for r in PR:
    assert r["holds"] is True, r
assert d["n_proto_rows_ok"]     == 3, d
assert d["proto_all_hold_ok"]   is True, d

assert 0.0 <= d["sigma_clock"] <= 1.0, d
assert d["sigma_clock"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v297: non-deterministic" >&2; exit 1; }
