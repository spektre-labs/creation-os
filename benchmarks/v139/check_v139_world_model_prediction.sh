#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v139 merge-gate:  linear latent world model
#
#   1. Self-test passes (fit + held-out prediction + surprise).
#   2. --demo emits a JSON object with σ_world array and monotone flag.
#   3. Held-out prediction error is < 10% (asserted inside the self-test
#      via the "held-out σ_world < 10%" check).
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v139_world_model}"
[[ -x "$BIN" ]] || { echo "[v139] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v139] 1/3 self-test"
"$BIN" --self-test >/tmp/cos_v139_self.log 2>&1 || {
    cat /tmp/cos_v139_self.log; exit 1; }

echo "[v139] 2/3 demo JSON shape"
OUT="$("$BIN" --demo)"
echo "$OUT" | tr -d '\n' | head -c 400; echo
echo "$OUT" | grep -q '"v139_demo":true'         || { echo "[v139] FAIL demo flag"; exit 1; }
echo "$OUT" | grep -q '"mean_l2_residual":'      || { echo "[v139] FAIL residual field"; exit 1; }
echo "$OUT" | grep -q '"sigma_world":\['         || { echo "[v139] FAIL sigma_world array"; exit 1; }
echo "$OUT" | grep -qE '"monotone_rising":(true|false)' \
                                                 || { echo "[v139] FAIL monotone flag"; exit 1; }

echo "[v139] 3/3 asserting held-out prediction <10% (checked in self-test)"
# Self-test exited 0, which already enforces the held-out <10% _CHECK.
# Additionally scrape the printed σ_world value from --demo to make the
# gate's accepted-magnitude explicit.
HELDOUT="$(echo "$OUT" | sed -nE 's/.*"sigma_world":\[([^]]+)\].*/\1/p' \
                       | awk -F',' '{max=$1; for(i=2;i<=NF;i++) if($i>max)max=$i; print max}')"
echo "[v139] rollout max σ_world = ${HELDOUT}"
awk "BEGIN{exit !(${HELDOUT} < 0.10)}" \
    || { echo "[v139] FAIL rollout max σ_world ≥ 10%"; exit 1; }
echo "[v139] PASS — linear world model fits + predicts + rolls out (held-out max σ_world = ${HELDOUT} < 0.10)"
