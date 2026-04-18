#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v147 merge-gate: σ-Reflect thought-trace analysis.
#
#   1. Self-test (R1 consistency / R2 weakest / R3 RAIN depth).
#   2. --demo on the canonical 4-step trace with pure answer "42":
#      consistency_ok=true, weakest_step=2 (σ=0.67), RAIN=full chain.
#   3. --divergence-demo on pure answer "WRONG":
#      consistency_ok=false, divergence_detected=true, weakest still
#      identified.
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v147_reflect}"
[[ -x "$BIN" ]] || { echo "[v147] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v147] 1/3 self-test"
"$BIN" --self-test >/tmp/cos_v147_self.log 2>&1 || {
    cat /tmp/cos_v147_self.log; exit 1; }

echo "[v147] 2/3 demo (consistent)"
OUT="$("$BIN" --demo)"
printf '%s\n' "${OUT:0:700}..."
echo "$OUT" | grep -q '"v147_demo":true' \
    || { echo "[v147] FAIL demo flag"; exit 1; }
echo "$OUT" | grep -q '"consistency_ok":true' \
    || { echo "[v147] FAIL consistency must hold on matching pure"; exit 1; }
echo "$OUT" | grep -q '"weakest_step":2' \
    || { echo "[v147] FAIL weakest_step must = 2"; exit 1; }
echo "$OUT" | grep -q '"rewind_depth":4' \
    || { echo "[v147] FAIL rewind_depth must = 4 on σ>0.60"; exit 1; }
echo "$OUT" | grep -q '"rain_should_rewind":true' \
    || { echo "[v147] FAIL RAIN should trigger"; exit 1; }

echo "[v147] 3/3 divergence-demo"
DOUT="$("$BIN" --divergence-demo)"
printf '%s\n' "${DOUT:0:700}..."
echo "$DOUT" | grep -q '"consistency_ok":false' \
    || { echo "[v147] FAIL divergence must flip consistency"; exit 1; }
echo "$DOUT" | grep -q '"divergence_detected":true' \
    || { echo "[v147] FAIL divergence_detected must be true"; exit 1; }
echo "$DOUT" | grep -q '"weakest_step":2' \
    || { echo "[v147] FAIL weakest_step still = 2 under divergence"; exit 1; }

echo "[v147] PASS — weakest + RAIN depth + divergence"
