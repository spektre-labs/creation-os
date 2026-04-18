#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v146 merge-gate: σ-Genesis template generator.
#
#   1. Self-test (generate → σ-gate → HITL approve/reject/pause).
#   2. --demo + --write TMPDIR: on disk layout has the canonical
#      four files, and kernel.h + kernel.c compile together cleanly
#      with $(CC).  (The generated test.c also compiles; we don't
#      run it, just prove the skeleton is coherent.)
#   3. --pause-demo: 3 consecutive rejections → paused=true.
#   4. Determinism: same seed ⇒ byte-identical state JSON
#      (excluding the wrote_files boolean).
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v146_genesis}"
[[ -x "$BIN" ]] || { echo "[v146] $BIN not built — run 'make $BIN'"; exit 2; }

echo "[v146] 1/4 self-test"
"$BIN" --self-test >/tmp/cos_v146_self.log 2>&1 || {
    cat /tmp/cos_v146_self.log; exit 1; }

echo "[v146] 2/4 generate → disk → compile check"
TMP="$(mktemp -d -t cos_v146_XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
OUT="$("$BIN" --demo --version 149 --name GenerateCanary \
             --seed 0xDEEDF00D --write "$TMP")"
printf '%s\n' "${OUT:0:700}..."
echo "$OUT" | grep -q '"v146_demo":true' \
    || { echo "[v146] FAIL demo flag missing"; exit 1; }
echo "$OUT" | grep -q '"wrote_files":true' \
    || { echo "[v146] FAIL wrote_files not true"; exit 1; }

KDIR="$TMP/v149"
for f in kernel.h kernel.c tests/test.c README.md; do
    [[ -s "$KDIR/$f" ]] \
        || { echo "[v146] FAIL missing or empty $f"; exit 1; }
done
echo "[v146] disk files:"
ls -la "$KDIR" "$KDIR/tests"

# Prove kernel.h + kernel.c compile into an object without errors.
echo "[v146] compiling kernel.{h,c} as a sanity check"
CC_BIN="${CC:-cc}"
"$CC_BIN" -O2 -Wall -std=c11 -c "$KDIR/kernel.c" -I "$KDIR" \
    -o "$TMP/kernel.o" \
    || { echo "[v146] FAIL generated kernel.c did not compile"; exit 1; }
[[ -s "$TMP/kernel.o" ]] \
    || { echo "[v146] FAIL generated object empty"; exit 1; }
echo "[v146] kernel.o size: $(wc -c < "$TMP/kernel.o") bytes"

# σ_code field must be present in state JSON and below τ=0.35.
SIG="$(echo "$OUT" | grep -oE '"sigma_code":[0-9.]+' | head -n1 | awk -F: '{print $2}')"
echo "[v146] σ_code = $SIG"
awk -v s="$SIG" 'BEGIN { exit (s+0 < 0.35) ? 0 : 1 }' \
    || { echo "[v146] FAIL σ_code not below τ_merge=0.35"; exit 1; }

echo "[v146] 3/4 pause-demo: 3 rejections → paused"
POUT="$("$BIN" --pause-demo)"
printf '%s\n' "${POUT:0:500}..."
echo "$POUT" | grep -q '"paused":true' \
    || { echo "[v146] FAIL expected paused=true after 3 rejections"; exit 1; }
echo "$POUT" | grep -q '"consecutive_rejects":3' \
    || { echo "[v146] FAIL consecutive_rejects != 3"; exit 1; }
REJ="$( (echo "$POUT" | grep -o '"status":"rejected"' || true) | wc -l | tr -d ' ')"
[[ "$REJ" -eq 3 ]] \
    || { echo "[v146] FAIL expected 3 rejected candidates"; exit 1; }

echo "[v146] 4/4 determinism (same seed → byte-identical state json)"
A="$("$BIN" --demo --seed 0xDEEDF00D --version 149 --name Canary)"
B="$("$BIN" --demo --seed 0xDEEDF00D --version 149 --name Canary)"
[[ "$A" == "$B" ]] \
    || { echo "[v146] FAIL same seed → different JSON"; exit 1; }

echo "[v146] PASS — generate + σ-gate + HITL + pause + compilable skeleton"
