#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v148 merge-gate: σ-Sovereign supervised loop.
#
#   1. Self-test (supervised + autonomous + emergency + unstable halt).
#   2. --run --mode supervised --cycles 6: library grows, genesis has
#      ≥ 1 PENDING candidate (no auto-approval).
#   3. --run --mode autonomous --cycles 6: genesis has ≥ 1 APPROVED
#      candidate.
#   4. --emergency-stop-after N short-circuits subsequent cycles.
#   5. Determinism: same seed → byte-identical JSON.
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v148_sovereign}"
[[ -x "$BIN" ]] || { echo "[v148] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v148] 1/5 self-test"
"$BIN" --self-test >/tmp/cos_v148_self.log 2>&1 || {
    cat /tmp/cos_v148_self.log; exit 1; }

echo "[v148] 2/5 supervised run (6 cycles)"
SUP="$("$BIN" --run --mode supervised --cycles 6 --seed 0x501EC08)"
printf '%s\n' "${SUP:0:800}..."
echo "$SUP" | grep -q '"v148_run":true' \
    || { echo "[v148] FAIL run tag missing"; exit 1; }

# Supervised: skills installed > 0
SK=$(echo "$SUP" | grep -oE '"skills":[0-9]+' | tail -n1 | awk -F: '{print $2}')
echo "[v148] supervised skills installed = $SK"
[[ "${SK:-0}" -ge 3 ]] \
    || { echo "[v148] FAIL expected ≥3 skills installed"; exit 1; }

# Supervised: pending approvals present (kernels_pending ≥ 1)
PEND=$(echo "$SUP" | grep -oE '"kernels_pending":[0-9]+' | tail -n1 | awk -F: '{print $2}')
echo "[v148] supervised kernels_pending = $PEND"
[[ "${PEND:-0}" -ge 1 ]] \
    || { echo "[v148] FAIL expected ≥1 pending kernel in supervised mode"; exit 1; }

# Supervised: kernels_approved must be 0 (no auto-approval)
APPR=$(echo "$SUP" | grep -oE '"kernels_approved":[0-9]+' | tail -n1 | awk -F: '{print $2}')
echo "[v148] supervised kernels_approved = $APPR"
[[ "${APPR:-0}" -eq 0 ]] \
    || { echo "[v148] FAIL supervised mode must NOT auto-approve"; exit 1; }

# reflect_weakest_step must be 2 on every cycle
REFL=$( (echo "$SUP" | grep -o '"reflect_weakest_step":2' || true) | wc -l | tr -d ' ')
[[ "$REFL" -ge 6 ]] \
    || { echo "[v148] FAIL reflect_weakest_step != 2 on some cycle"; exit 1; }

echo "[v148] 3/5 autonomous run (6 cycles)"
AUT="$("$BIN" --run --mode autonomous --cycles 6 --seed 0x501EC08)"
printf '%s\n' "${AUT:0:600}..."
APPR_A=$(echo "$AUT" | grep -oE '"kernels_approved":[0-9]+' | tail -n1 | awk -F: '{print $2}')
echo "[v148] autonomous kernels_approved = $APPR_A"
[[ "${APPR_A:-0}" -ge 1 ]] \
    || { echo "[v148] FAIL autonomous mode must auto-approve ≥1 kernel"; exit 1; }

echo "[v148] 4/5 emergency-stop-after 3"
STOP="$("$BIN" --run --cycles 6 --seed 0x501EC08 --emergency-stop-after 3)"
ES=$( (echo "$STOP" | grep -o '"emergency_stopped":true' || true) | wc -l | tr -d ' ')
echo "[v148] emergency_stopped lines = $ES"
[[ "$ES" -ge 3 ]] \
    || { echo "[v148] FAIL expected post-stop cycles to short-circuit"; exit 1; }
echo "$STOP" | grep -q '"final_state":.*"emergency_stopped":true' \
    || { echo "[v148] FAIL final state not emergency_stopped"; exit 1; }

echo "[v148] 5/5 determinism (same seed → byte-identical JSON)"
A="$("$BIN" --run --mode supervised --cycles 5 --seed 0xDEEDF00D)"
B="$("$BIN" --run --mode supervised --cycles 5 --seed 0xDEEDF00D)"
[[ "$A" == "$B" ]] \
    || { echo "[v148] FAIL same seed → different JSON"; exit 1; }

echo "[v148] PASS — supervised + autonomous + emergency-stop + deterministic"
