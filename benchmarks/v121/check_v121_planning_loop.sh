#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v121 σ-Planning merge-gate smoke.
#
# What this check enforces:
#   - self-test green (3-step happy path + pathological abort)
#   - /v1/plan JSON shape: goal, plan[], total_sigma, max_sigma,
#     replans, aborted
#   - replans ≥ 2 on the happy synthetic (branches 0 and 1 fail σ,
#     branch 2 commits)
#   - every committed step has σ_step ≤ τ_step
#
# Real tool execution (v112 / v113 / v114 wiring) is v121.1.

set -euo pipefail

BIN=${BIN:-./creation_os_v121_planning}
if [ ! -x "$BIN" ]; then
    echo "check-v121: $BIN not found; run \`make creation_os_v121_planning\`" >&2
    exit 1
fi

echo "check-v121: self-test"
"$BIN" --self-test

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

echo "check-v121: /v1/plan JSON shape"
"$BIN" --plan "Analyze CSV and create summary report" > "$tmp"

grep -q '"goal":'        "$tmp" || { cat "$tmp"; echo "missing goal";        exit 1; }
grep -q '"plan":\['      "$tmp" || { cat "$tmp"; echo "missing plan[]";      exit 1; }
grep -q '"total_sigma":' "$tmp" || { cat "$tmp"; echo "missing total_sigma"; exit 1; }
grep -q '"max_sigma":'   "$tmp" || { cat "$tmp"; echo "missing max_sigma";   exit 1; }
grep -q '"replans":'     "$tmp" || { cat "$tmp"; echo "missing replans";     exit 1; }
grep -q '"aborted":false' "$tmp" || { cat "$tmp"; echo "expected aborted=false"; exit 1; }

# At least one replan must have occurred.
replans=$(sed -E 's/.*"replans":([0-9]+).*/\1/' "$tmp")
[ "$replans" -ge 2 ] || { echo "expected replans ≥ 2, got $replans"; exit 1; }

# Max σ must be ≤ 0.60 (τ_step default) — otherwise a non-compliant
# step was committed.
max_sigma=$(sed -E 's/.*"max_sigma":([0-9.]+).*/\1/' "$tmp")
python3 - <<PY
import sys
ms = float("$max_sigma")
if ms > 0.60:
    print(f"max_sigma {ms} > τ_step 0.60", file=sys.stderr); sys.exit(1)
PY

grep -q '"tool":"memory"' "$tmp"  || { echo "expected tool=memory committed at step 0"; exit 1; }
grep -q '"replanned":true' "$tmp" || { echo "expected at least one replanned=true step"; exit 1; }

echo "check-v121: OK (HTN + MCTS backtrack on σ > τ; /v1/plan JSON contract)"
