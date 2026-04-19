#!/usr/bin/env bash
#
# v250 σ-A2A — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * Agent Card: name="Creation OS", 6 capabilities in order
#     (reason, plan, create, simulate, teach, coherence),
#     sigma_profile.{mean,calibration} in [0,1],
#     presence=="LIVE", scsl==true, non-empty endpoint,
#     card_ok==true
#   * 4 task delegation fixtures; decision matches σ vs
#     τ_neg (0.50) and τ_refuse (0.75); >=1 ACCEPT AND
#     >=1 NEGOTIATE AND >=1 REFUSE
#   * 3 federation partners in order (alice, bob, carol),
#     every σ_trust in [0,1]
#   * sigma_a2a in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v250_a2a"
[ -x "$BIN" ] || { echo "v250: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v250", d
assert d["chain_valid"] is True, d

c = d["agent_card"]
assert c["name"] == "Creation OS", c
want_caps = ["reason","plan","create","simulate","teach","coherence"]
assert c["capabilities"] == want_caps, c
assert c["n_capabilities"] == 6, c
sp = c["sigma_profile"]
assert 0.0 <= sp["mean"]        <= 1.0, c
assert 0.0 <= sp["calibration"] <= 1.0, c
assert c["presence"] == "LIVE", c
assert c["scsl"] is True, c
assert len(c["endpoint"]) > 0, c
assert c["card_ok"] is True, c

tn  = d["tau_neg"]
trf = d["tau_refuse"]
assert abs(tn  - 0.50) < 1e-6, d
assert abs(trf - 0.75) < 1e-6, d
for t in d["tasks"]:
    assert 0.0 <= t["sigma_product"] <= 1.0, t
    if   t["sigma_product"] > trf: exp = "REFUSE"
    elif t["sigma_product"] > tn : exp = "NEGOTIATE"
    else:                           exp = "ACCEPT"
    assert t["decision"] == exp, t
assert d["n_accept"]    >= 1, d
assert d["n_negotiate"] >= 1, d
assert d["n_refuse"]    >= 1, d
assert d["n_accept"] + d["n_negotiate"] + d["n_refuse"] == len(d["tasks"]), d

pn = [p["name"] for p in d["partners"]]
assert pn == ["alice","bob","carol"], pn
for p in d["partners"]:
    assert 0.0 <= p["sigma_trust"] <= 1.0, p
assert d["n_partners_ok"] == 3, d

assert 0.0 <= d["sigma_a2a"] <= 1.0, d
assert d["sigma_a2a"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v250: non-deterministic" >&2; exit 1; }
