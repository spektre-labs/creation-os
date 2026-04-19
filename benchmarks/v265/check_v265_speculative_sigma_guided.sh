#!/usr/bin/env bash
#
# v265 σ-Speculative — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 2 models: role draft + verifier (canonical)
#   * 4 bands with spec_len [12,8,6,4] (canonical,
#     strictly non-increasing); sigma_hi > sigma_lo;
#     monotone_ok
#   * 3 multi-draft duels; winner == argmin(sigma);
#     >=1 A-win AND >=1 B-win
#   * 4 σ-gate fixtures; decision matches σ vs
#     τ_spec=0.35 (≤→ACCEPT else REJECT);
#     >=1 ACCEPT AND >=1 REJECT
#   * throughput: plain < sigma_spec AND speedup_x >= 2.0
#   * sigma_speculative in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v265_speculative"
[ -x "$BIN" ] || { echo "v265: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v265", d
assert d["chain_valid"] is True, d
assert abs(d["tau_spec"] - 0.35) < 1e-6, d

roles = [m["role"] for m in d["models"]]
assert roles == ["draft","verifier"], roles
assert d["n_models_ok"] == 2, d

want = [12, 8, 6, 4]
assert [b["spec_len"] for b in d["bands"]] == want, d
prev = 999
for b in d["bands"]:
    assert b["sigma_hi"] > b["sigma_lo"], b
    assert b["spec_len"] <= prev, b
    prev = b["spec_len"]
assert d["monotone_ok"] is True
assert d["n_bands_ok"] == 4, d

a_wins = b_wins = 0
for x in d["duels"]:
    exp = "A" if x["sigma_a"] < x["sigma_b"] else "B"
    assert x["winner"] == exp, x
    if x["winner"] == "A": a_wins += 1
    if x["winner"] == "B": b_wins += 1
assert a_wins >= 1 and b_wins >= 1, (a_wins, b_wins)
assert d["n_duels_ok"] == 3, d
assert d["n_a_wins"] == a_wins, d
assert d["n_b_wins"] == b_wins, d

tau = d["tau_spec"]
a = r = 0
for g in d["gates"]:
    exp = "ACCEPT" if g["sigma_speculated"] <= tau else "REJECT"
    assert g["decision"] == exp, g
    if g["decision"] == "ACCEPT": a += 1
    else: r += 1
assert a >= 1 and r >= 1, (a, r)
assert d["n_accept"] == a and d["n_reject"] == r, d

th = d["throughput"]
assert th["tok_per_s_plain"] < th["tok_per_s_sigma_spec"], th
assert th["speedup_x"] >= 2.0, th
assert th["speedup_ok"] is True, th

assert 0.0 <= d["sigma_speculative"] <= 1.0, d
assert d["sigma_speculative"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v265: non-deterministic" >&2; exit 1; }
