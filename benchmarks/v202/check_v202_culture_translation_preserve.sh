#!/usr/bin/env bash
#
# v202 σ-Culture — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 6 profiles × 6 cores = 36 translations
#   * every translation: σ_translate < τ_drift
#   * symbol invariance ≥ 90 %
#   * ≥ 1 rephrase (taboo gate fired, surface non-empty)
#   * σ_offense_mean_after < σ_offense_mean_before
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v202_culture"
[ -x "$BIN" ] || { echo "v202: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]      == "v202", d
assert d["n_profiles"]  == 6,       d
assert d["n_cores"]     == 6,       d
assert len(d["translations"]) == 36, d
assert d["n_below_drift"]     == 36, d
assert d["n_symbol_preserved_full"] * 10 >= 36 * 9, d
assert d["n_rephrased_total"] >= 1, d
assert d["chain_valid"]       is True, d
assert d["sigma_offense_mean_after"] < d["sigma_offense_mean_before"], d

tau_drift = d["tau_drift"]
tau_off   = d["tau_offense"]
for t in d["translations"]:
    assert t["drift"] < tau_drift, t
    if t["rephrased"]:
        assert t["surface_len"] > 0, t  # never censored: non-empty surface
    assert 0.0 <= t["offense"] <= 1.0, t
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v202: non-deterministic output" >&2
    exit 1
fi
