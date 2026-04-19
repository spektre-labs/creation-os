#!/usr/bin/env bash
#
# v234 σ-Presence — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_instances == 10, tau_drift == 0.30, refresh_period == 8
#   * every state in {SEED, FORK, RESTORED, SUCCESSOR, LIVE}
#     appears at least once
#   * every instance has σ_drift ∈ [0, 1]
#   * n_honest >= 5 AND n_drifting >= 5 AND sum == n_instances
#   * honest   ⇔ σ_drift == 0
#   * drifting ⇒ σ_drift >= tau_drift
#   * every instance emits a valid `X-COS-Presence: <STATE>`
#     header and passes the identity-refresh stub
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v234_presence"
[ -x "$BIN" ] || { echo "v234: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v234", d
assert d["chain_valid"] is True, d
assert d["n_instances"] == 10, d
assert abs(d["tau_drift"] - 0.30) < 1e-6, d
assert d["refresh_period"] == 8, d

sc = d["state_counts"]
for k in ("SEED", "FORK", "RESTORED", "SUCCESSOR", "LIVE"):
    assert sc[k] >= 1, (k, sc)

tau = d["tau_drift"]
n_honest = n_drifting = 0
states = {"SEED","FORK","RESTORED","SUCCESSOR","LIVE"}
for it in d["instances"]:
    assert it["declared"] in states, it
    assert it["actual"]   in states, it
    assert 0.0 <= it["sigma_drift"] <= 1.0, it
    if it["honest"]:
        assert it["sigma_drift"] == 0.0, it
        n_honest += 1
    else:
        assert it["sigma_drift"] >= tau - 1e-6, it
        n_drifting += 1
    assert it["assertion_ok"] is True, it
    assert it["refresh_valid"] is True, it
    assert it["assertion"].startswith("X-COS-Presence: "), it
    assert it["assertion"].split(": ", 1)[1] == it["declared"], it

assert n_honest   == d["n_honest"]   >= 5, d
assert n_drifting == d["n_drifting"] >= 5, d
assert n_honest + n_drifting == d["n_instances"], d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v234: non-deterministic" >&2; exit 1; }
