#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v140 merge-gate: counterfactual propagation + σ-channel attribution.
#
#   1. Self-test passes (cf scaling + top-3 attribution + JSON shape).
#   2. --demo emits a JSON with attribution.top of length 3 and a
#      counterfactual block with a non-zero sigma_causal.
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v140_causal}"
[[ -x "$BIN" ]] || { echo "[v140] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v140] 1/3 self-test"
"$BIN" --self-test >/tmp/cos_v140_self.log 2>&1 || {
    cat /tmp/cos_v140_self.log; exit 1; }

echo "[v140] 2/3 demo JSON"
OUT="$("$BIN" --demo)"
echo "$OUT" | tr -d '\n' | head -c 600; echo
echo "$OUT" | grep -q '"v140_demo":true'    || { echo "[v140] FAIL demo flag"; exit 1; }
echo "$OUT" | grep -q '"counterfactual":'   || { echo "[v140] FAIL counterfactual block"; exit 1; }
echo "$OUT" | grep -q '"attribution":'      || { echo "[v140] FAIL attribution block"; exit 1; }
# top-3 attribution: 3 "index" entries in the "top" array
TOPN="$(echo "$OUT" | grep -oE '"index":[0-9]+' | wc -l | tr -d ' ')"
echo "[v140] top-N entries seen = $TOPN (≥3 required)"
[[ "$TOPN" -ge 3 ]] || { echo "[v140] FAIL top-3 attribution"; exit 1; }

# counterfactual must produce a non-zero sigma_causal for the demo
SCAUS="$(echo "$OUT" | sed -nE 's/.*"sigma_causal":([0-9.eE+-]+).*/\1/p')"
echo "[v140] counterfactual sigma_causal = ${SCAUS}"
awk "BEGIN{exit !(${SCAUS} > 0.0)}" \
    || { echo "[v140] FAIL sigma_causal == 0 for nonzero intervention"; exit 1; }

echo "[v140] 3/3 verdict field"
echo "$OUT" | grep -qE '"verdict":"(emit|abstain)"' \
    || { echo "[v140] FAIL verdict shape"; exit 1; }

echo "[v140] PASS — counterfactual Δ + σ-channel top-3 attribution"
