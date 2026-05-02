#!/usr/bin/env bash
#
# v276 σ-Gated-DeltaNet — merge-gate check (backends + σ-gate + combo + tri-backend tasks).
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v276_gated_deltanet"
[ -x "$BIN" ] || { echo "v276: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v276", d
assert abs(d["tau_gate"] - 0.35) < 1e-5, d
assert d["denominator"] == 18, d
assert d["passing"] == 18, d
assert abs(d["sigma_deltanet"]) < 1e-5, d
assert d["manifest_closed"] is True, d

B = d["backends"]
assert len(B) == 2
assert B[0]["name"] == "deltanet" and B[0]["exponent"] == 1 and B[0]["gate_mechanism"] is True
assert B[1]["name"] == "transformer" and B[1]["exponent"] == 2 and B[1]["gate_mechanism"] is False
assert B[0]["throughput_rel"] > B[1]["throughput_rel"], B

G = d["gates"]
assert len(G) == 4
assert all(g["expect_ok"] for g in G), G
lin = sum(1 for g in G if g["decision"] == "LINEAR")
fb = sum(1 for g in G if g["decision"] == "FALLBACK_FULL")
assert lin >= 1 and fb >= 1, G

C = d["combo"]
assert len(C) == 3
assert C[0]["component"] == "deltanet" and C[0]["layer_slot"] == 1 and C[0]["enabled"] is True
assert C[1]["component"] == "ttt" and C[1]["layer_slot"] == 2 and C[1]["enabled"] is True
assert C[2]["component"] == "sigma_gate" and C[2]["layer_slot"] == 3 and C[2]["enabled"] is True

T = d["tasks"]
assert len(T) == 3
chosen = [t["chosen"] for t in T]
assert len(set(chosen)) >= 2, chosen
for t in T:
    sm, sd, st = t["sigma_mamba"], t["sigma_deltanet"], t["sigma_ttt"]
    sigs = {"mamba": sm, "deltanet": sd, "ttt": st}
    assert t["chosen"] in sigs
    assert abs(t["sigma_chosen"] - sigs[t["chosen"]]) < 1e-4
    others = [v for k, v in sigs.items() if k != t["chosen"]]
    assert abs(t["sigma_rival"] - min(others)) < 1e-4
    assert t["sigma_chosen"] <= t["sigma_rival"] + 1e-4

assert d["n_task_rows_ok"] == 3
assert d["task_chosen_ok"] is True
assert d["chain_hash"].startswith("0x"), d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v276: non-deterministic JSON" >&2; exit 1; }
