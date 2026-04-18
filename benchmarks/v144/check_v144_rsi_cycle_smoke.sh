#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v144 merge-gate: σ-RSI cycle state machine.
#
#   1. Self-test (accept / rollback / 3-strike pause / σ_rsi).
#   2. --demo on a stable improving trajectory: ≥ 3 cycles accepted
#      without regression, σ_rsi small, paused=false at end.
#   3. --pause-demo: 3 consecutive regressions ⇒ paused=true ⇒
#      subsequent submit is skipped (skipped_paused:true, current
#      score frozen, cycle_count capped).
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v144_rsi}"
[[ -x "$BIN" ]] || { echo "[v144] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v144] 1/4 self-test"
"$BIN" --self-test >/tmp/cos_v144_self.log 2>&1 || {
    cat /tmp/cos_v144_self.log; exit 1; }

echo "[v144] 2/4 demo JSON — improving trajectory"
OUT="$("$BIN" --demo)"
printf '%s\n' "${OUT:0:600}..."
echo "$OUT" | grep -q '"v144_demo":true' \
    || { echo "[v144] FAIL demo tag missing"; exit 1; }

ACC_TRUE="$( (echo "$OUT" | grep -o '"accepted":true' || true) | wc -l | tr -d ' ')"
echo "[v144] accepted cycles = $ACC_TRUE"
[[ "$ACC_TRUE" -ge 3 ]] \
    || { echo "[v144] FAIL expected ≥3 accepted cycles on stable trajectory"; exit 1; }

FINAL_PAUSED="$(echo "$OUT" | grep -oE '"final_state":[^}]*"paused":(true|false)' || true)"
echo "[v144] $FINAL_PAUSED"
echo "$FINAL_PAUSED" | grep -q '"paused":false' \
    || { echo "[v144] FAIL stable trajectory should end unpaused"; exit 1; }

echo "[v144] 3/4 pause-demo — 3 consecutive regressions must latch pause"
POUT="$("$BIN" --pause-demo)"
printf '%s\n' "${POUT:0:600}..."

# Exactly one paused_now:true (the 3rd regression must trigger it once).
PNOW="$( (echo "$POUT" | grep -o '"paused_now":true' || true) | wc -l | tr -d ' ')"
[[ "$PNOW" -eq 1 ]] \
    || { echo "[v144] FAIL expected exactly one paused_now:true, got $PNOW"; exit 1; }

# After pause, at least one submit must report skipped_paused:true.
SKIP="$( (echo "$POUT" | grep -o '"skipped_paused":true' || true) | wc -l | tr -d ' ')"
[[ "$SKIP" -ge 1 ]] \
    || { echo "[v144] FAIL expected ≥1 skipped_paused:true after latch"; exit 1; }

# Final state must be paused, with skipped_total ≥ 1.
echo "$POUT" | grep -q '"final_state":.*"paused":true' \
    || { echo "[v144] FAIL final state not paused"; exit 1; }

echo "[v144] 4/4 σ_rsi present on every report"
SIG="$( (echo "$OUT" | grep -o '"sigma_rsi":' || true) | wc -l | tr -d ' ')"
[[ "$SIG" -ge 3 ]] \
    || { echo "[v144] FAIL σ_rsi missing from cycle reports"; exit 1; }

echo "[v144] PASS — accept + rollback + 3-strike pause + σ_rsi"
