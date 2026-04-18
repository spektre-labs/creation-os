#!/usr/bin/env bash
#
# v184 σ-VLA — merge-gate check.
#
# Contracts:
#   * self-test PASSES;
#   * grounding accuracy ≥ 8/10 scenes;
#   * at least one ambiguous scene detected and aborted
#     (dual-system safety gate fired);
#   * every σ channel ∈ [0,1] and dual σ ≥ any single σ;
#   * output byte-deterministic across two runs.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v184_vla"
[ -x "$BIN" ] || { echo "v184: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json, sys
d = json.loads("""$OUT""")

assert d["kernel"] == "v184",       d["kernel"]
assert d["n_scenes"] == 10,         d["n_scenes"]
assert d["n_correct"] >= 8,         f"correct={d['n_correct']}/10"
assert d["n_ambiguous"] >= 1,       d["n_ambiguous"]
assert d["n_aborted"] >= 1,         d["n_aborted"]
assert d["n_aborted"] == d["n_asked_human"], (d["n_aborted"], d["n_asked_human"])

# σ channel bounds
for s in d["scenes"]:
    for k in ("sigma_perception","sigma_plan","sigma_action",
             "sigma_grounding","sigma_dual"):
        v = s[k]
        assert 0.0 <= v <= 1.0, (k, v, s)
    # dual ≥ single channels (soft floor)
    for k in ("sigma_perception","sigma_plan","sigma_action"):
        assert s["sigma_dual"] + 1e-4 >= s[k], (k, s)

# Every aborted scene must have at least one channel > tau_gate
tau = d["tau_gate"]
for s in d["scenes"]:
    if s["aborted"]:
        assert any(s[k] > tau for k in
                   ("sigma_perception","sigma_plan",
                    "sigma_action","sigma_grounding")), s

# Every non-aborted scene should execute via System 1 i.e. low all-channel σ
for s in d["scenes"]:
    if not s["aborted"]:
        assert s["sigma_action"] <= tau, s
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v184: non-deterministic output" >&2
    exit 1
fi
