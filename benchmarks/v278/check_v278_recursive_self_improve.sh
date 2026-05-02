#!/usr/bin/env bash
#
# v278 σ-Recursive-Self-Improve — merge-gate.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v278_rsi"
[ -x "$BIN" ] || { echo "v278: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v278"
assert abs(d["tau_goedel"] - 0.40) < 1e-5
assert d["denominator"] == 21 and d["passing"] == 21
assert abs(d["sigma_rsi"]) < 1e-5
assert d["manifest_closed"] is True

E = d["calibration_err"]
assert len(E) == 4
for i in range(3):
    assert E[i] > E[i + 1] + 1e-6
assert E[-1] <= 0.05 + 1e-5
assert d["calibration_ok"] is True

A = d["arch"]
assert len(A) == 3
chosen = [x for x in A if x["chosen"]]
assert len(chosen) == 1
best = max(A, key=lambda x: x["aurcc"])
assert best["chosen"] is True
aurccs = [x["aurcc"] for x in A]
assert len(set(round(x, 5) for x in aurccs)) >= 2

T = d["thresholds"]
assert len(T) == 3
want = {"code": 0.20, "creative": 0.50, "medical": 0.15}
for row in T:
    assert row["domain"] in want
    assert abs(row["tau"] - want[row["domain"]]) < 1e-4
    assert 0 < row["tau"] < 1
taus = [row["tau"] for row in T]
assert len(set(round(x, 5) for x in taus)) >= 2

G = d["goedel"]
assert len(G) == 3 and all(x["expect_ok"] for x in G)
acts = [x["action"] for x in G]
assert "SELF_CONFIDENT" in acts and "CALL_PROCONDUCTOR" in acts
assert d["arch_argmax_ok"] and d["tau_distinct_ok"] and d["godel_both_ok"]
assert d["chain_hash"].startswith("0x")
PY

A="$("$BIN")"
B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v278: non-deterministic JSON" >&2; exit 1; }
