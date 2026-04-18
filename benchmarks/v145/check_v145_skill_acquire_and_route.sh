#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v145 merge-gate: σ-Skill library.
#
#   1. Self-test (acquire / route / stack / share / evolve).
#   2. --demo on seed 0xC05 seeds 5 skills, stacks math+code,
#      shares at τ=0.35, evolves math at least once.
#   3. --route math and --route code both hit the library.
#   4. --route bogus misses cleanly (no crash, hit=false).
#   5. Determinism: same seed ⇒ byte-identical JSON.
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v145_skill}"
[[ -x "$BIN" ]] || { echo "[v145] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v145] 1/5 self-test"
"$BIN" --self-test >/tmp/cos_v145_self.log 2>&1 || {
    cat /tmp/cos_v145_self.log; exit 1; }

echo "[v145] 2/5 demo JSON (seed 0xC05)"
OUT="$("$BIN" --demo --seed 0xC05)"
printf '%s\n' "${OUT:0:700}..."
echo "$OUT" | grep -q '"v145_demo":true' \
    || { echo "[v145] FAIL demo tag missing"; exit 1; }
echo "$OUT" | grep -q '"n_skills":5' \
    || { echo "[v145] FAIL expected 5 installed skills"; exit 1; }
echo "$OUT" | grep -q '"evolved_math":true' \
    || { echo "[v145] FAIL expected math skill evolved once"; exit 1; }

SHARED="$(echo "$OUT" | grep -oE '"shared_at_0.35":[0-9]+' | awk -F':' '{print $2}')"
echo "[v145] shared count = $SHARED"
[[ "$SHARED" -ge 1 ]] \
    || { echo "[v145] FAIL expected ≥1 shareable skill"; exit 1; }

echo "[v145] 3/5 route math + code"
RM="$("$BIN" --route math --seed 0xC05)"
echo "$RM" | grep -q '"hit":true' \
    || { echo "[v145] FAIL math route missed"; exit 1; }
echo "$RM" | grep -q '"topic":"math"' \
    || { echo "[v145] FAIL math route payload bad"; exit 1; }
RC="$("$BIN" --route code --seed 0xC05)"
echo "$RC" | grep -q '"hit":true' \
    || { echo "[v145] FAIL code route missed"; exit 1; }

echo "[v145] 4/5 route bogus topic must miss cleanly"
RB="$("$BIN" --route metaphysics --seed 0xC05)"
echo "$RB" | grep -q '"hit":false' \
    || { echo "[v145] FAIL bogus route should report hit=false"; exit 1; }

echo "[v145] 5/5 determinism"
A="$("$BIN" --demo --seed 0xC05)"
B="$("$BIN" --demo --seed 0xC05)"
[[ "$A" == "$B" ]] \
    || { echo "[v145] FAIL same seed → different JSON"; exit 1; }

echo "[v145] PASS — acquire + σ-gate + route + stack + share + evolve + deterministic"
