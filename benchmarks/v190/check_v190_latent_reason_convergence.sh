#!/usr/bin/env bash
#
# v190 σ-Latent-Reason — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * σ_latent strictly decreasing across iterations on every query
#   * ≥ 90 % of queries reach σ_latent < τ_converge (0.01)
#   * hard queries consume ≥ 3× more iterations than easy queries
#   * zero intermediate tokens emitted (total_middle_tokens == 0)
#   * byte-deterministic output
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v190_latent"
[ -x "$BIN" ] || { echo "v190: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]               == "v190",  d
assert d["n_queries"]            == 32,      d
assert d["tau_converge"]         <= 0.01,    d
assert d["monotone_sigma_all"]   is True,    d
assert d["total_middle_tokens"]  == 0,       d

# ≥ 90 % converge.
assert d["n_converged"] >= (d["n_queries"] * 9) // 10, d

# Each query: if converged, min_sigma_latent < τ_converge.
for q in d["queries"]:
    assert q["n_middle_tokens"] == 0, q
    if q["converged"]:
        assert q["min_sigma_latent"] < d["tau_converge"], q

# Hard / easy iteration ratio.
assert d["mean_iters_hard"] >= 3.0 * d["mean_iters_easy"], d
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v190: non-deterministic output" >&2
    exit 1
fi
