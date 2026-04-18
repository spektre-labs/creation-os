#!/usr/bin/env bash
#
# v209 σ-Containment — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 12 action proposals, ≥ 1 block at each of
#     layers 1..5 (sandbox / action-gate /
#     constitutional / audit / intent)
#   * at least one terminator block after kill-switch
#   * no ALLOWED action has σ_intent>τ_intent,
#     σ_concealment>τ_conceal, or σ_action>τ_action
#   * no ALLOWED network/DNS without user_grant
#     (network default-CLOSED)
#   * every ALLOWED action carries pre+post log and they
#     match byte-for-byte (no concealment)
#   * terminated == true after kill-switch event
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v209_containment"
[ -x "$BIN" ] || { echo "v209: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v209", d
assert d["n"] == 12, d
assert d["network_default_open"] is False, d
assert d["terminated"] is True, d
assert d["chain_valid"] is True, d

plb = d["per_layer_blocks"]
# layers 1..5 must each have ≥ 1 block; layer 6 (terminator) ≥ 1
for L in (1, 2, 3, 4, 5, 6):
    assert plb[L] >= 1, (L, plb)

tau_i = d["tau_intent"]; tau_c = d["tau_conceal"]; tau_a = d["tau_action"]
for a in d["actions"]:
    for f in ("si", "sc", "sa"):
        assert 0.0 <= a[f] <= 1.0 + 1e-6, (f, a)
    if a["verdict"] == 0:   # ALLOWED
        assert a["si"] <= tau_i + 1e-6, a
        assert a["sc"] <= tau_c + 1e-6, a
        assert a["sa"] <= tau_a + 1e-6, a
        assert a["pre"] and a["post"] and a["match"], a
        # Network default-closed: NETWORK=2, DNS=4
        if a["op"] in (2, 4):
            assert a["grant"] is True, a
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v209: non-deterministic" >&2; exit 1; }
