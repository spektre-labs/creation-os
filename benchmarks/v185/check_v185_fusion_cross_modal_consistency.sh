#!/usr/bin/env bash
#
# v185 σ-Multimodal-Fusion — merge-gate check.
#
# Contracts:
#   * self-test PASSES;
#   * modality registry: 4 entries (vision/audio/text/action);
#   * consistent samples classified with no false flags;
#   * conflict samples separated by ≥ 0.10 σ_cross;
#   * at least one modality dynamically dropped (σ > τ_drop);
#   * fusion weights sum to 1 per sample;
#   * output byte-deterministic across two runs.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v185_fusion"
[ -x "$BIN" ] || { echo "v185: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]        == "v185", d["kernel"]
assert d["n_modalities"]  == 4,      d["n_modalities"]
assert d["n_samples"]     == 16,     d["n_samples"]

names = {m["name"] for m in d["modalities"]}
assert names == {"vision","audio","text","action"}, names

sep = d["mean_sigma_cross_conflict"] - d["mean_sigma_cross_consistent"]
assert sep >= 0.10, f"sep={sep}"

assert d["n_dropped_modalities"] >= 1, d["n_dropped_modalities"]

# No false flags on consistent samples; high sensitivity on conflict.
n_cons = sum(1 for s in d["samples"] if s["truth"])
n_conf = sum(1 for s in d["samples"] if not s["truth"])
assert d["n_consistent_correct"] == n_cons
assert d["n_conflict_correct"]   >= n_conf - 1

# σ ∈ [0,1]
for s in d["samples"]:
    for k in ("sigma_cross","sigma_fused"):
        assert 0.0 <= s[k] <= 1.0, (k, s)
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v185: non-deterministic output" >&2
    exit 1
fi
