#!/usr/bin/env bash
# σ-Plan smoke test (A2): budget + execute + replan + abort.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_plan
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Demo plan: 3 steps, 4 allowed, read-class budget, all executed.
grep -q '"goal":"demo: list + count + math"' <<<"$OUT" || { echo "goal"       >&2; exit 5; }
grep -q '"max_steps":4'                       <<<"$OUT" || { echo "max_steps" >&2; exit 6; }
grep -q '"max_cost_eur":0.0010'               <<<"$OUT" || { echo "max_cost"  >&2; exit 7; }
grep -q '"tau_replan":0.6000'                 <<<"$OUT" || { echo "tau"       >&2; exit 8; }
grep -q '"n_steps":3'                         <<<"$OUT" || { echo "n_steps"   >&2; exit 9; }
grep -q '"spent_eur":0.0003'                  <<<"$OUT" || { echo "spent"     >&2; exit 10; }
grep -q '"status":"OK"'                       <<<"$OUT" || { echo "status"    >&2; exit 11; }
grep -q '"exec_count":3'                      <<<"$OUT" || { echo "exec ct"   >&2; exit 12; }
# Step-level receipts: each step σ_post is 0.05, completed.
grep -q '"sigma_post":0.0500,"done":true'     <<<"$OUT" || { echo "step done" >&2; exit 13; }

echo "check-sigma-plan: PASS (3-step budgeted plan, σ_plan computed, all done)"
