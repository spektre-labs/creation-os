#!/usr/bin/env bash
#
# v288 σ-Oculus — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 3 cascade rows canonical (medical TIGHT 0.10 /
#     code NORMAL 0.30 / creative OPEN 0.60); 3
#     DISTINCT widths; tau strictly increasing
#   * 3 extreme fixtures (closed useless/not
#     dangerous; open dangerous/not useless;
#     optimal neither)
#   * 3 adaptive steps with error_rate strictly
#     decreasing; action TIGHTEN iff error >
#     threshold_error=0.05 else STABLE; at least 1
#     TIGHTEN AND at least 1 STABLE; when action
#     TIGHTEN at step n, tau_{n+1} < tau_n
#   * 3 transparency fields (tau_declared,
#     sigma_measured, decision_visible) all
#     reported=true
#   * sigma_oculus in [0,1] AND == 0.0
#   * deterministic JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v288_oculus"
[ -x "$BIN" ] || { echo "v288: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v288", d
assert d["chain_valid"] is True, d
assert abs(d["threshold_error"] - 0.05) < 1e-6, d

C = d["cascade"]
want = [("medical",0.10,"TIGHT"), ("code",0.30,"NORMAL"),
        ("creative",0.60,"OPEN")]
for (dom,t,w), r in zip(want, C):
    assert r["domain"] == dom, r
    assert abs(r["tau"] - t) < 1e-5, r
    assert r["width"] == w, r
assert len({r["width"] for r in C}) == 3, C
taus = [r["tau"] for r in C]
assert taus[0] < taus[1] < taus[2], taus
assert d["n_cascade_rows_ok"]         == 3, d
assert d["cascade_width_distinct_ok"] is True, d
assert d["cascade_tau_monotone_ok"]   is True, d

E = d["extreme"]
want_e = [("closed",True,False), ("open",False,True),
          ("optimal",False,False)]
for (lbl,u,g), r in zip(want_e, E):
    assert r["label"] == lbl, r
    assert r["useless"]   is u, r
    assert r["dangerous"] is g, r
assert d["n_extreme_rows_ok"]  == 3, d
assert d["extreme_polarity_ok"] is True, d

A = d["adaptive"]
errs = [r["error_rate"] for r in A]
assert all(errs[i] > errs[i+1] for i in range(len(errs)-1)), errs
for i, r in enumerate(A):
    exp = "TIGHTEN" if r["error_rate"] > d["threshold_error"] else "STABLE"
    assert r["action"] == exp, r
    if r["action"] == "TIGHTEN" and i+1 < len(A):
        assert A[i+1]["tau"] < r["tau"], (r, A[i+1])
n_t = sum(1 for r in A if r["action"] == "TIGHTEN")
n_s = sum(1 for r in A if r["action"] == "STABLE")
assert n_t >= 1 and n_s >= 1, (n_t, n_s)
assert d["n_adaptive_rows_ok"]           == 3, d
assert d["adaptive_error_monotone_ok"]   is True, d
assert d["adaptive_action_rule_ok"]      is True, d
assert d["n_action_tighten"]             == n_t, d
assert d["n_action_stable"]              == n_s, d

T = d["transparent"]
want_t = ["tau_declared", "sigma_measured", "decision_visible"]
assert [r["field"] for r in T] == want_t, T
for r in T:
    assert r["reported"] is True, r
assert d["n_transparent_rows_ok"]         == 3, d
assert d["transparency_all_reported_ok"]  is True, d

assert 0.0 <= d["sigma_oculus"] <= 1.0, d
assert d["sigma_oculus"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v288: non-deterministic" >&2; exit 1; }
