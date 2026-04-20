#!/usr/bin/env bash
#
# v301 σ-ZKP — merge-gate check.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v301_zkp"
[ -x "$BIN" ] || { echo "v301: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v301_zkp", d
assert d["chain_valid"] is True, d

P = d["proofs"]
want = ["well_formed_proof", "edge_case_proof", "forged_proof"]
assert [r["label"] for r in P] == want, P
prev = -1.0
for r in P:
    assert r["sigma_proof"] > prev, P
    prev = r["sigma_proof"]
TAU = d["tau_proof"]
assert abs(TAU - 0.40) < 1e-6, d
for r in P:
    assert r["valid"] is (r["sigma_proof"] <= TAU), r
    assert r["reveals_raw"] is False, r
n_v = sum(1 for r in P if r["valid"])
n_i = sum(1 for r in P if not r["valid"])
assert n_v >= 1 and n_i >= 1, P
assert d["proof_rows_ok"]       == 3,    d
assert d["proof_order_ok"]      is True, d
assert d["proof_polarity_ok"]   is True, d
assert d["proof_no_reveal_ok"]  is True, d

R = d["roles"]
want_r = ["client", "cloud", "verifier"]
assert [r["role"] for r in R] == want_r, R
assert R[0]["sees_raw_input"]     is False, R[0]
assert R[0]["sees_model_weights"] is False, R[0]
assert R[0]["sees_answer"]        is True,  R[0]
assert R[0]["sees_sigma"]         is True,  R[0]
assert R[0]["sees_proof"]         is True,  R[0]
assert R[1]["sees_raw_input"]     is True,  R[1]
assert R[1]["sees_model_weights"] is True,  R[1]
assert R[2]["sees_raw_input"]     is False, R[2]
assert R[2]["sees_model_weights"] is False, R[2]
assert R[2]["sees_answer"]        is False, R[2]
assert R[2]["sees_sigma"]         is True,  R[2]
assert R[2]["sees_proof"]         is True,  R[2]
assert d["role_rows_ok"]              == 3,    d
assert d["role_client_hidden_ok"]     is True, d
assert d["role_verifier_hidden_ok"]   is True, d
assert d["zk_privacy_holds"]          is True, d

I = d["integrity"]
want_i = ["advertised_served", "silent_downgrade", "advertised_match"]
assert [r["scenario"] for r in I] == want_i, I
TAU_I = d["tau_integrity"]
assert abs(TAU_I - 0.50) < 1e-6, d
for r in I:
    assert r["detected_mismatch"] is (r["sigma_integrity"] > TAU_I), r
    assert r["verdict"] == ("DETECTED" if r["detected_mismatch"] else "OK"), r
n_ok = sum(1 for r in I if not r["detected_mismatch"])
n_de = sum(1 for r in I if r["detected_mismatch"])
assert n_ok == 2 and n_de == 1, I
assert d["integrity_rows_ok"]       == 3,    d
assert d["integrity_polarity_ok"]   is True, d
assert d["integrity_count_ok"]      is True, d

S = d["scsl"]
want_s = ["allowed_purpose_a", "allowed_purpose_b", "disallowed_purpose"]
assert [r["policy"] for r in S] == want_s, S
TAU_P = d["tau_policy"]
assert abs(TAU_P - 0.50) < 1e-6, d
for r in S:
    assert r["attested"] is (r["sigma_policy"] <= TAU_P), r
    assert r["purpose_revealed"] is False, r
n_a = sum(1 for r in S if r["attested"])
n_r = sum(1 for r in S if not r["attested"])
assert n_a >= 1 and n_r >= 1, S
assert d["scsl_rows_ok"]      == 3,    d
assert d["scsl_polarity_ok"]  is True, d
assert d["scsl_no_reveal_ok"] is True, d

assert 0.0 <= d["sigma_zkp"] <= 1.0, d
assert d["sigma_zkp"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v301: non-deterministic" >&2; exit 1; }
