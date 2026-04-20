#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_swarm
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0'           <<<"$OUT" || { echo "rc != 0"       >&2; exit 3; }
grep -q '"pass":true'                <<<"$OUT" || { echo "pass != true"  >&2; exit 4; }

# Canonical 4-model demo: σ = [0.60, 0.20, 0.40, 0.30].
# weights = [0.40, 0.80, 0.60, 0.70] → mean_w = 0.625 → ACCEPT.
# Winner = idx 1 (claude-haiku, σ=0.20).
grep -q '"verdict":"ACCEPT"'         <<<"$OUT" || { echo "verdict != ACCEPT" >&2; exit 5; }
grep -q '"winner_idx":1'             <<<"$OUT" || { echo "winner_idx != 1"   >&2; exit 6; }
grep -q '"winner_model":"claude-haiku"' <<<"$OUT" || { echo "winner model"   >&2; exit 7; }
grep -q '"winner_sigma":0.2000'      <<<"$OUT" || { echo "winner_sigma"      >&2; exit 8; }
grep -q '"mean_weight":0.6250'       <<<"$OUT" || { echo "mean_weight"       >&2; exit 9; }
grep -q '"sigma_swarm":0.3750'       <<<"$OUT" || { echo "sigma_swarm"      >&2; exit 10; }
grep -q '"total_cost_eur":0.0170'    <<<"$OUT" || { echo "total_cost"       >&2; exit 11; }

# Escalation ladder: bitnet σ=0.60 → haiku cuts to 0.20 → stop.
# So started_with=1, added=1 (only haiku), final_used=2.
grep -q '"added":1'                  <<<"$OUT" || { echo "added != 1"       >&2; exit 12; }
grep -q '"final_used":2'             <<<"$OUT" || { echo "final_used != 2" >&2; exit 13; }

echo "check-sigma-swarm: PASS (4-model consensus: ACCEPT @ haiku σ=0.20, €0.017; ladder added 1)"
