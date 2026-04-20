#!/usr/bin/env bash
# σ-Meta smoke test (S5): per-domain competence + weakest focus.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_meta
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Canonical demo: 4 domains, 3 observations each = 12 samples.
grep -q '"count":4'               <<<"$OUT" || { echo "count"     >&2; exit 5; }
grep -q '"total_samples":12'      <<<"$OUT" || { echo "total"     >&2; exit 6; }
grep -q '"overall_mean_sigma":0.4742' <<<"$OUT" || { echo "overall" >&2; exit 7; }

# Per-domain σ_mean pinned (3-observation averages).
grep -q '"name":"factual_qa","sigma_mean":0.1767,"competence":"STRONG"' <<<"$OUT" \
    || { echo "factual"  >&2; exit 8; }
grep -q '"name":"math","sigma_mean":0.6500,"competence":"WEAK"'         <<<"$OUT" \
    || { echo "math"     >&2; exit 9; }
grep -q '"name":"code","sigma_mean":0.3500,"competence":"MODERATE"'     <<<"$OUT" \
    || { echo "code"     >&2; exit 10; }
grep -q '"name":"commonsense","sigma_mean":0.7200,"competence":"LIMITED"' <<<"$OUT" \
    || { echo "common"   >&2; exit 11; }

# Weakest/strongest indices + focus recommendation.
grep -q '"strongest":"factual_qa"'  <<<"$OUT" || { echo "strongest" >&2; exit 12; }
grep -q '"weakest":"commonsense"'   <<<"$OUT" || { echo "weakest"   >&2; exit 13; }
grep -q '"focus":"commonsense"'     <<<"$OUT" || { echo "focus"     >&2; exit 14; }

echo "check-sigma-meta: PASS (per-domain σ_mean + competence flags + focus)"
