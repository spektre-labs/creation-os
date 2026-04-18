#!/usr/bin/env bash
# v154 σ-Showcase merge-gate: runs all three demo scenarios
# start-to-finish, checks σ-abstain gate behavior, and asserts
# byte-level determinism for a fixed (scenario, seed, τ).
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

BIN=./creation_os_v154_showcase
SEED=154

die() { echo "v154 check: FAIL — $*" >&2; exit 1; }

echo "  self-test"
st="$($BIN --self-test)"
echo "  $st"
echo "$st" | grep -q '"self_test":"pass"' || die "self-test"

echo "  research demo"
research="$($BIN --scenario research --seed "$SEED" --tau 0.60)"
echo "$research" | grep -q '"scenario":"research"'     || die "research scenario"
echo "$research" | grep -q '"n_stages":8'              || die "research n_stages"
echo "$research" | grep -q '"abstained":false'         || die "research should emit"
echo "$research" | grep -q '"kernel":"v118"'           || die "v118 stage missing"
echo "$research" | grep -q '"kernel":"v152"'           || die "v152 stage missing"
echo "$research" | grep -q '"kernel":"v153"'           || die "v153 stage missing"

echo "  coder demo"
coder="$($BIN --scenario coder --seed "$SEED" --tau 0.60)"
echo "$coder" | grep -q '"scenario":"coder"'           || die "coder scenario"
echo "$coder" | grep -q '"n_stages":6'                 || die "coder n_stages"
echo "$coder" | grep -q '"kernel":"v151"'              || die "v151 stage missing"
echo "$coder" | grep -q '"kernel":"v113"'              || die "v113 stage missing"
echo "$coder" | grep -q '"kernel":"v147"'              || die "v147 stage missing"

echo "  collab demo"
collab="$($BIN --scenario collab --seed "$SEED" --tau 0.60)"
echo "$collab" | grep -q '"scenario":"collab"'         || die "collab scenario"
echo "$collab" | grep -q '"n_stages":4'                || die "collab n_stages"
echo "$collab" | grep -q '"kernel":"v128"'             || die "v128 stage missing"
echo "$collab" | grep -q '"kernel":"v129"'             || die "v129 stage missing"

echo "  abstain fires under tight τ"
tight="$($BIN --scenario research --seed "$SEED" --tau 0.05)"
echo "$tight" | grep -q '"abstained":true'             || die "abstain gate never fired"
echo "$tight" | grep -q '"abstain_stage_index":0'      || die "abstain index"

echo "  abstain-free under wide τ"
wide="$($BIN --scenario coder --seed "$SEED" --tau 0.80)"
echo "$wide" | grep -q '"abstained":false'             || die "wide τ should emit"

echo "  determinism (same seed → byte-identical JSON)"
a="$($BIN --scenario research --seed "$SEED" --tau 0.60)"
b="$($BIN --scenario research --seed "$SEED" --tau 0.60)"
[ "$a" = "$b" ] || die "non-deterministic"

echo "  demo-all runs all three"
all="$($BIN --demo-all --seed "$SEED")"
n_lines=$(printf '%s\n' "$all" | wc -l | tr -d ' ')
[ "$n_lines" = "3" ] || die "demo-all produced $n_lines lines (want 3)"

echo "v154 showcase smoke: OK"
