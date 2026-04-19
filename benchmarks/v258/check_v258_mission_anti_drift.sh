#!/usr/bin/env bash
#
# v258 σ-Mission — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * mission_statement matches the canonical string
#     byte-for-byte; statement_ok
#   * exactly 4 impact scopes in canonical order
#     (per_user, per_domain, per_institution,
#     per_society); σ_before, σ_after in [0,1];
#     delta_sigma = σ_before − σ_after; every
#     improved==true; n_scopes_improved == 4
#   * exactly 4 anti-drift rows; decision matches
#     σ_mission vs τ_mission=0.30 (≤ → ACCEPT, > →
#     REJECT); >=1 ACCEPT AND >=1 REJECT
#   * exactly 4 long-term anchors in canonical order
#     (civilization_governance, wisdom_transfer_legacy,
#     human_sovereignty, declared_eq_realized) with
#     upstreams v203, v233, v238, 1=1; all anchor_ok;
#     n_anchors_ok == 4
#   * sigma_mission in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v258_mission"
[ -x "$BIN" ] || { echo "v258: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v258", d
assert d["chain_valid"] is True, d
assert abs(d["tau_mission"] - 0.30) < 1e-6, d

MS = ("Reduce sigma in every system that touches human life. "
      "Make intelligence trustworthy.")
assert d["mission_statement"] == MS, d["mission_statement"]
assert d["statement_ok"] is True, d

sn = [r["scope"] for r in d["scopes"]]
assert sn == ["per_user","per_domain","per_institution","per_society"], sn
for r in d["scopes"]:
    assert 0.0 <= r["sigma_before"] <= 1.0, r
    assert 0.0 <= r["sigma_after"]  <= 1.0, r
    assert abs(r["delta_sigma"] - (r["sigma_before"] - r["sigma_after"])) < 1e-6, r
    assert r["improved"] is True, r
assert d["n_scopes_improved"] == 4, d

tau = d["tau_mission"]
for row in d["drift"]:
    assert 0.0 <= row["sigma_mission"] <= 1.0, row
    exp = "ACCEPT" if row["sigma_mission"] <= tau else "REJECT"
    assert row["decision"] == exp, row
assert d["n_accept"] >= 1, d
assert d["n_reject"] >= 1, d
assert d["n_accept"] + d["n_reject"] == 4, d

want_anchors = [
    ("civilization_governance", "v203"),
    ("wisdom_transfer_legacy",  "v233"),
    ("human_sovereignty",       "v238"),
    ("declared_eq_realized",    "1=1"),
]
got = [(a["anchor"], a["upstream"]) for a in d["anchors"]]
assert got == want_anchors, got
for a in d["anchors"]:
    assert a["anchor_ok"] is True, a
assert d["n_anchors_ok"] == 4, d

assert 0.0 <= d["sigma_mission"] <= 1.0, d
assert d["sigma_mission"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v258: non-deterministic" >&2; exit 1; }
