#!/usr/bin/env bash
#
# v198 σ-Moral — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * all 4 frameworks scored on every decision
#   * ≥ 4 clear (σ_moral_mean < τ_clear) and
#     ≥ 4 dilemma (σ_moral_mean > τ_dilemma) decisions
#   * geodesic selection strictly minimises path σ mean
#   * dilemmas emit honest_uncertainty=true; clear cases don't
#   * zero firmware refusals on clear cases
#   * hash chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v198_moral"
[ -x "$BIN" ] || { echo "v198: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]        == "v198", d
assert d["n_decisions"]   == 16,      d
assert d["n_frameworks"]  == 4,       d
assert d["n_clear"]       >= 4, d
assert d["n_dilemma"]     >= 4, d
assert d["n_firmware_refusals"] == 0, d
assert d["chain_valid"]   is True, d

tau_c = d["tau_clear"]
tau_d = d["tau_dilemma"]

for dec in d["decisions"]:
    assert len(dec["final_scores"]) == 4, dec
    means = dec["path_means"]
    assert len(means) == 3, dec
    # Geodesic is strictly minimum.
    selected_mean = means[dec["selected_path"]]
    for k, m in enumerate(means):
        if k == dec["selected_path"]:
            continue
        assert m > selected_mean, (k, m, selected_mean, dec)

    sig = dec["sigma_moral_mean"]
    if sig >= tau_d:
        assert dec["honest"] is True, dec
    if sig < tau_c:
        assert dec["honest"] is False, dec
        assert dec["firmware_refusal"] is False, dec

# Dilemma = honest cases match n_honest
assert d["n_honest"] >= 4, d
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v198: non-deterministic output" >&2
    exit 1
fi
