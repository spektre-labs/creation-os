#!/usr/bin/env bash
#
# v183 σ-Governance-Theory — merge-gate check.
#
# Contracts:
#   * self-test enumerates all 14 properties and passes all of
#     them (7 axioms + 3 liveness + 4 safety);
#   * total state count ≥ 100 000 (bounded model claim);
#   * deterministic JSON output;
#   * TLA+ spec file exists at specs/v183/governance_theory.tla.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v183_governance"
[ -x "$BIN" ] || { echo "v183: $BIN not built" >&2; exit 1; }

if [ ! -f specs/v183/governance_theory.tla ]; then
    echo "v183: specs/v183/governance_theory.tla missing" >&2
    exit 1
fi

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json, sys
d = json.loads("""$OUT""")
assert d["n_props"]   == 14, f"n_props={d['n_props']}"
assert d["n_passed"]  == 14, f"passed={d['n_passed']}"
assert d["n_failed"]  == 0,  f"failed={d['n_failed']}"
assert d["total_states"] >= 1000000, f"states={d['total_states']}"

kinds = {"axiom": 0, "liveness": 0, "safety": 0}
for p in d["properties"]:
    assert p["passed"] is True, f"property failed: {p}"
    assert p["counter"] == 0,   f"counterexamples: {p}"
    kinds[p["kind"]] = kinds.get(p["kind"], 0) + 1
assert kinds == {"axiom": 7, "liveness": 3, "safety": 4}, kinds

names = {p["name"] for p in d["properties"]}
required = {
    "A1_sigma_in_unit_interval","A2_emit_requires_low_sigma",
    "A3_abstain_requires_block","A4_learn_monotone",
    "A5_forget_intact","A6_steer_reduces_sigma",
    "A7_consensus_byzantine","L1_progress_always",
    "L2_rsi_improves_one_domain","L3_heal_recovers",
    "S1_no_silent_failure","S2_no_unchecked_output",
    "S3_no_private_leak","S4_no_regression_propagates",
}
missing = required - names
assert not missing, f"missing properties: {missing}"
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v183: non-deterministic output" >&2
    exit 1
fi
