#!/usr/bin/env bash
#
# v229 σ-Seed — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * seed = 5 canonical kernels {29, 101, 106, 124, 148}
#   * n_accepted ≥ 15, n_rejected ≥ 1
#   * every accepted kernel has σ_growth ≤ τ_growth
#   * every accepted non-seed kernel has its declared
#     parent already accepted at integration time
#   * terminal_hash == terminal_hash_verify (determ.)
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v229_seed"
[ -x "$BIN" ] || { echo "v229: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v229", d
assert d["chain_valid"] is True, d
assert d["terminal_hash"] == d["terminal_hash_verify"], d

seed_ids = [k["version"] for k in d["pool"] if k["is_seed"]]
assert seed_ids == [29, 101, 106, 124, 148], seed_ids

assert d["n_accepted"] >= 15, d
assert d["n_rejected"] >= 1, d

tau = d["tau_growth"]
accepted_ids = set()
for k in d["pool"]:
    if k["is_seed"]:
        accepted_ids.add(k["version"])
for k in d["pool"]:
    if k["is_seed"]: continue
    if k["accepted"]:
        assert k["sigma_growth"] <= tau + 1e-6, k
        assert k["parent"] in accepted_ids, k
        accepted_ids.add(k["version"])

assert d["n_accepted"] == len(accepted_ids), (d["n_accepted"], len(accepted_ids))
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v229: non-deterministic" >&2; exit 1; }
