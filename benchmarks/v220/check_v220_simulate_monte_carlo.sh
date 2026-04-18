#!/usr/bin/env bash
#
# v220 σ-Simulate — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 4-state system, 4 rules each (baseline + whatif)
#   * every σ_rule ≤ τ_rule (0.10); σ_engine ≤ τ_rule
#   * Monte Carlo: 200 rollouts × 8 steps each, per
#     scenario; terminal histograms sum to n_mc
#   * σ_sim_baseline, σ_sim_whatif ∈ [0, 1]
#   * histograms DIFFER (whatif perturbs an outcome)
#   * σ_causal ∈ [0, 1] per state; Σ σ_causal ≤ 2
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v220_simulate"
[ -x "$BIN" ] || { echo "v220: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v220", d
assert d["n_states"] == 4 and d["n_rules"] == 4, d
assert d["n_mc"] == 200 and d["n_steps"] == 8, d
assert sum(d["histogram_baseline"]) == d["n_mc"], d
assert sum(d["histogram_whatif"])   == d["n_mc"], d

for r in d["rules_baseline"] + d["rules_whatif"]:
    assert r["sigma_rule"] <= d["tau_rule"] + 1e-6, r

assert d["sigma_engine"] <= d["tau_rule"] + 1e-6, d
assert 0.0 <= d["sigma_sim_baseline"] <= 1.0 + 1e-6, d
assert 0.0 <= d["sigma_sim_whatif"]   <= 1.0 + 1e-6, d

assert d["histogram_baseline"] != d["histogram_whatif"], d

for c in d["sigma_causal"]:
    assert 0.0 <= c <= 1.0 + 1e-6, c
assert sum(d["sigma_causal"]) <= 2.0 + 1e-5, d
assert d["chain_valid"] is True, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v220: non-deterministic" >&2; exit 1; }
