#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v141 merge-gate: σ-Curriculum cycle.
#
#   1. Self-test (weakness detection + improvement + no forgetting).
#   2. --demo emits a JSON array of cycles; the weakest topic must
#      strictly improve each cycle, and strong_topics_stable must be
#      true throughout.
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v141_curriculum}"
[[ -x "$BIN" ]] || { echo "[v141] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v141] 1/3 self-test"
"$BIN" --self-test >/tmp/cos_v141_self.log 2>&1 || {
    cat /tmp/cos_v141_self.log; exit 1; }

echo "[v141] 2/3 demo JSON — 5 cycles, α=0.4"
OUT="$("$BIN" --demo --cycles 5 --alpha 0.4)"
printf '%s\n' "${OUT:0:800}..."
echo "$OUT" | grep -q '"v141_demo":true' \
    || { echo "[v141] FAIL demo flag"; exit 1; }

# Every cycle must report strong_topics_stable:true.  grep returns 1
# on no-match, which pipefail would propagate — guard with `|| true`.
STABLE_TRUE="$( (echo "$OUT" | grep -o '"strong_topics_stable":true'  || true) | wc -l | tr -d ' ')"
STABLE_FALSE="$((echo "$OUT" | grep -o '"strong_topics_stable":false' || true) | wc -l | tr -d ' ')"
echo "[v141] strong_topics_stable: ${STABLE_TRUE} true, ${STABLE_FALSE} false"
[[ "$STABLE_TRUE" -ge 5 && "$STABLE_FALSE" -eq 0 ]] \
    || { echo "[v141] FAIL forgetting detected in at least one cycle"; exit 1; }

# Each cycle's improvement field must be positive (strict progress).
IMPROVE_POS="$(echo "$OUT" | grep -oE '"improvement":[0-9.eE+-]+' | \
    awk -F':' '{if ($2 > 0.0) ok++} END{print ok+0}')"
echo "[v141] cycles with improvement>0: ${IMPROVE_POS} / 5"
[[ "$IMPROVE_POS" -ge 5 ]] \
    || { echo "[v141] FAIL a cycle did not produce positive improvement"; exit 1; }

echo "[v141] 3/3 asserting first-cycle weakest = history (spec default roster)"
FIRST="$(echo "$OUT" | head -c 300)"
echo "$FIRST" | grep -q '"weakest":"history"' \
    || { echo "[v141] FAIL expected first-cycle weakest=history"; exit 1; }

echo "[v141] PASS — weakness detection + improvement + no forgetting"
