#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v151 merge-gate: σ-code-agent TDD cycle (test → impl → compile → run → σ-gate).
set -euo pipefail

BIN=./creation_os_v151_code_agent
[[ -x "$BIN" ]] || { echo "missing binary $BIN"; exit 1; }

WORK=$(mktemp -d 2>/dev/null || mktemp -d -t v151)
trap 'rm -rf "$WORK"' EXIT

# -----------------------------------------------------------------
# T1: self-test passes (structural pause-latch + JSON contracts).
# -----------------------------------------------------------------
out=$("$BIN" --self-test)
echo "  self-test: $out"
echo "$out" | grep -q '"self_test":"pass"' || { echo "FAIL: self-test"; exit 1; }

# -----------------------------------------------------------------
# T2: full TDD cycle — Phase A must fail (test w/o impl), Phase B
#     must compile (kernel.c + test.c), Phase C must exit 0.
# -----------------------------------------------------------------
cycle=$("$BIN" --tdd --version 900 --name probe --gap "synthetic gap" \
               --workroot "$WORK" --tau 0.40 --seed 151)
echo "  tdd cycle: $cycle"
echo "$cycle" | grep -q '"phase_a_fail_ok":true'      || { echo "FAIL: Phase A unexpected pass"; exit 1; }
echo "$cycle" | grep -q '"phase_b_compile_ok":true'   || { echo "FAIL: Phase B compile broken"; exit 1; }
echo "$cycle" | grep -q '"phase_c_test_pass":true'    || { echo "FAIL: Phase C run failed"; exit 1; }
echo "$cycle" | grep -q '"status":"pending"'          || { echo "FAIL: σ_code did not gate pending"; exit 1; }

# -----------------------------------------------------------------
# T3: σ_code composite is ≈ 0.1 (geomean of three OK legs).
# -----------------------------------------------------------------
sc=$(echo "$cycle" | sed -E 's/.*"sigma_code_composite":([0-9.]+).*/\1/')
awk -v s="$sc" 'BEGIN{exit !(s+0 < 0.20)}' \
    || { echo "FAIL: σ_code_composite=$sc not ≤ 0.20"; exit 1; }

# -----------------------------------------------------------------
# T4: generated files exist on disk (4 canonical emit).
# -----------------------------------------------------------------
for f in kernel.h kernel.c tests/test.c README.md tdd.log; do
    [[ -s "$WORK/v900/$f" ]] || { echo "FAIL: missing $WORK/v900/$f"; exit 1; }
done
[[ -x "$WORK/v900/full" ]] || { echo "FAIL: Phase B binary missing"; exit 1; }

# -----------------------------------------------------------------
# T5: σ-gate paths — force GATED_OUT with τ = 0.05 (below OK/√3).
# -----------------------------------------------------------------
gated=$("$BIN" --tdd --version 901 --name probe --gap "gated gap" \
               --workroot "$WORK" --tau 0.05 --seed 151)
echo "$gated" | grep -q '"status":"gated_out"' \
    || { echo "FAIL: τ=0.05 should force gated_out: $gated"; exit 1; }

# -----------------------------------------------------------------
# T6: pause-demo latches paused=true after 3 rejects.
# -----------------------------------------------------------------
pause=$("$BIN" --pause-demo --workroot "$WORK")
echo "  pause demo: $pause"
echo "$pause" | grep -q '"paused":true' || { echo "FAIL: paused not latched"; exit 1; }

# -----------------------------------------------------------------
# T7: determinism — same seed + same workroot clone → same JSON.
# -----------------------------------------------------------------
WORK2=$(mktemp -d 2>/dev/null || mktemp -d -t v151b)
trap 'rm -rf "$WORK" "$WORK2"' EXIT
a=$("$BIN" --tdd --version 777 --name det --gap "det gap" --workroot "$WORK2" --tau 0.40 --seed 7)
WORK3=$(mktemp -d 2>/dev/null || mktemp -d -t v151c)
trap 'rm -rf "$WORK" "$WORK2" "$WORK3"' EXIT
b=$("$BIN" --tdd --version 777 --name det --gap "det gap" --workroot "$WORK3" --tau 0.40 --seed 7)
# Strip workdir/log paths before comparing (tmp dir differs).
a_strip=$(echo "$a" | sed -E 's/"workdir":"[^"]+"/"workdir":"_"/;s/"log":"[^"]+"/"log":"_"/')
b_strip=$(echo "$b" | sed -E 's/"workdir":"[^"]+"/"workdir":"_"/;s/"log":"[^"]+"/"log":"_"/')
[[ "$a_strip" == "$b_strip" ]] || { echo "FAIL: v151 not deterministic"; exit 1; }

echo "v151 code-agent TDD smoke: OK"
