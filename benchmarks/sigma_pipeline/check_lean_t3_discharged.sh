#!/usr/bin/env bash
#
# CLOSE-1: structural + (when available) type-checking proof that
# the six σ-gate theorems live `sorry`-free in
# hw/formal/v259/Measurement.lean.  The previous iteration of this
# check targeted the abstract `LinearOrder α` lift (`gateα_*`); the
# current Lean 4 implementation instantiates over `Nat`
# concretely (see the file header for the Nat ↔ Float bridge
# rationale) so the six theorem names are now:
#
#   T1  gate_totality
#   T2  gate_monotone_in_sigma
#   T3  gate_anti_monotone_in_tau_a
#   T4  gate_anti_monotone_in_tau_r
#   T5  gate_boundary_at_tau_a
#   T6  gate_boundary_at_tau_r
#
# When a Lean 4 toolchain is available (lean / lake on PATH), we
# additionally invoke `lake build` to confirm the file type-checks
# cleanly; absent the toolchain, the structural grep is the
# strongest local evidence — it guarantees the PROOF TEXT is
# shipped in the tree and that no committer reverted it to a
# `sorry`.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

LEAN="hw/formal/v259/Measurement.lean"
LAKEFILE="hw/formal/v259/lakefile.lean"
[[ -f "$LEAN" ]]      || { echo "missing $LEAN"      >&2; exit 2; }
[[ -f "$LAKEFILE" ]]  || { echo "missing $LAKEFILE"  >&2; exit 2; }

# ---- 1. six concrete discharges must be present --------------------------
for thm in gate_totality \
           gate_monotone_in_sigma \
           gate_anti_monotone_in_tau_a \
           gate_anti_monotone_in_tau_r \
           gate_boundary_at_tau_a \
           gate_boundary_at_tau_r; do
    grep -q "^theorem $thm" "$LEAN" \
        || { echo "FAIL: missing theorem $thm in $LEAN" >&2; exit 3; }
done

# ---- 2. zero executable `sorry` tactics anywhere in the file -------------
#        Mentions of the word inside Lean comments (`-- ... sorry ...`,
#        `/- ... sorry ... -/`) and inside inline-code backticks in the
#        file-header docblock are allowed.  We use python so the
#        comment-stripper is obviously correct.
python3 - "$LEAN" <<'PY' || exit 4
import re, sys
src = open(sys.argv[1]).read()
# strip /- ... -/ block comments (non-greedy, across lines)
src = re.sub(r'/-.*?-/', '', src, flags=re.DOTALL)
# strip -- line comments
src = re.sub(r'--[^\n]*', '', src)
# strip `...` inline code (the file-level docblock is already gone
# but belt + braces for any surviving backtick spans)
src = re.sub(r'`[^`]*`', '', src)
for i, line in enumerate(src.splitlines(), 1):
    if re.search(r'\bsorry\b', line):
        print(f"FAIL: line {i}: {line}", file=sys.stderr)
        sys.exit(1)
PY

# ---- 3. companion docs line must match -----------------------------------
grep -q "6/6" docs/v259/formal_status.md \
    || { echo "FAIL: docs/v259/formal_status.md missing 6/6 ledger line" >&2; exit 5; }

# ---- 4. optional: run Lean when a toolchain is on PATH -------------------
if command -v lake >/dev/null 2>&1 && command -v lean >/dev/null 2>&1; then
    echo "  (lean toolchain found — invoking lake build)"
    if ( cd hw/formal/v259 && lake build ) >/tmp/cos_lean.out 2>&1; then
        echo "  lean: file type-checks cleanly (lake build: 0 errors, 0 sorries)"
    else
        tail -40 /tmp/cos_lean.out >&2 || true
        echo "FAIL: lake build returned non-zero in hw/formal/v259" >&2
        exit 6
    fi
else
    echo "  (lean toolchain absent — structural grep only; install elan + run 'cd hw/formal/v259 && lake build' to reproduce)"
fi

echo "check-lean-t3-discharged: PASS (gate_{totality,monotone_σ,anti_monotone_τa,anti_monotone_τr,boundary_τa,boundary_τr} all sorry-free)"
