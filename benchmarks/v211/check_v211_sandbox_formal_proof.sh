#!/usr/bin/env bash
#
# v211 σ-Sandbox-Formal — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4 Frama-C proofs, all proved, σ_proof ≤ τ_proof
#   * every TLA+ invariant explored ≥ 10^6 states with
#     ZERO violations
#   * every attack-tree leaf names a non-zero
#     blocked_by proof id
#   * every certification σ_cert_ready ∈ [0,1]
#   * σ_formal ≤ τ_proof
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v211_sandbox_formal"
[ -x "$BIN" ] || { echo "v211: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v211", d
assert d["n_proved"] == d["n_proofs"] == 4, d
assert d["n_violations_total"] == 0, d
assert d["sigma_formal"] <= d["tau_proof"] + 1e-6, d
assert d["chain_valid"] is True, d

for p in d["proofs"]:
    assert p["proved"] is True, p
    assert p["s"] <= d["tau_proof"] + 1e-6, p
for inv in d["invariants"]:
    assert inv["states"] >= d["min_states_required"], inv
    assert inv["violations"] == 0, inv
for atk in d["attacks"]:
    assert atk["blocked_by"] != 0, atk
for c in d["certs"]:
    assert 0.0 <= c["s"] <= 1.0 + 1e-6, c
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v211: non-deterministic" >&2; exit 1; }
