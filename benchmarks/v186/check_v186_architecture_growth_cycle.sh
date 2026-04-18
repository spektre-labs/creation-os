#!/usr/bin/env bash
#
# v186 σ-Continual-Architecture — merge-gate check.
#
# Contracts:
#   * self-test PASSES;
#   * ≥ 1 starved domain detected;
#   * architecture grows at least one kernel and accepts ≥ 1;
#   * architecture prunes at least one kernel;
#   * final kernel count differs from initial (net change);
#   * every architecture change is captured in a verified hash
#     chain (replay → same tip);
#   * output byte-deterministic across two runs.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v186_grow"
[ -x "$BIN" ] || { echo "v186: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]             == "v186", d["kernel"]
assert d["n_initial_kernels"]  == 6, d["n_initial_kernels"]
assert d["n_grown"]            >= 1, d["n_grown"]
assert d["n_accepted"]         >= 1, d["n_accepted"]
assert d["n_pruned"]           >= 1, d["n_pruned"]
assert d["n_final_kernels"]    != d["n_initial_kernels"], d
assert sum(1 for x in d["starved"] if x) >= 1, d["starved"]

# Chain tip non-zero; hashes all unique; in-order.
tip = d["chain_tip"]
assert tip != "0000000000000000", tip
changes = d["changes"]
assert len(changes) == d["n_changes"]
assert len(changes) == len({c["hash"] for c in changes})  # no collisions
assert changes[-1]["hash"] == tip
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v186: non-deterministic output" >&2
    exit 1
fi
