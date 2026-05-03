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
LEAN133="formal/lean/CreationOS/V133.lean"
LAKE133="formal/lean/lakefile.lean"
[[ -f "$LEAN" ]]      || { echo "missing $LEAN"      >&2; exit 2; }
[[ -f "$LAKEFILE" ]]  || { echo "missing $LAKEFILE"  >&2; exit 2; }
[[ -f "$LEAN133" ]]   || { echo "missing $LEAN133"   >&2; exit 2; }
[[ -f "$LAKE133" ]]   || { echo "missing $LAKE133"   >&2; exit 2; }

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

for thm in engram_stores_only_accept \
           abstain_does_not_propagate \
           cascade_monotone \
           circuit_breaker_trips \
           proconductor_overrides_all \
           kv_eviction_removes_highest_sigma \
           speculative_skip_safe \
           staleness_increases_sigma; do
    grep -q "^theorem $thm" "$LEAN133" \
        || { echo "FAIL: missing theorem $thm in $LEAN133" >&2; exit 3; }
done

# ---- 2. zero executable `sorry` tactics anywhere in the file -------------
#        Mentions of the word inside Lean comments (`-- ... sorry ...`,
#        `/- ... sorry ... -/`) and inside inline-code backticks in the
#        file-header docblock are allowed.  We use python so the
#        comment-stripper is obviously correct.
strip_check () {
    local f=$1
python3 - "$f" <<'PY' || exit 4
import re, sys
src = open(sys.argv[1]).read()
src = re.sub(r'/-.*?-/', '', src, flags=re.DOTALL)
src = re.sub(r'--[^\n]*', '', src)
src = re.sub(r'`[^`]*`', '', src)
for i, line in enumerate(src.splitlines(), 1):
    if re.search(r'\bsorry\b', line):
        print(f"FAIL: {sys.argv[1]} line {i}: {line}", file=sys.stderr)
        sys.exit(1)
PY
}

strip_check "$LEAN"
strip_check "$LEAN133"

# ---- 3. companion docs line must match -----------------------------------
grep -q "14/14" docs/v259/formal_status.md \
    || { echo "FAIL: docs/v259/formal_status.md missing 14/14 ledger line" >&2; exit 5; }

# ---- 4. optional: run Lean when a toolchain is on PATH -------------------
if command -v lake >/dev/null 2>&1 && command -v lean >/dev/null 2>&1; then
    echo "  (lean toolchain found — invoking lake build)"
    if ( cd hw/formal/v259 && lake build ) >/tmp/cos_lean.out 2>&1; then
        echo "  lean v259: OK"
    else
        tail -40 /tmp/cos_lean.out >&2 || true
        echo "FAIL: lake build returned non-zero in hw/formal/v259" >&2
        exit 6
    fi
    if ( cd formal/lean && lake build ) >/tmp/cos_lean133.out 2>&1; then
        echo "  lean v133: OK (CreationOS.V133)"
    else
        tail -40 /tmp/cos_lean133.out >&2 || true
        echo "FAIL: lake build returned non-zero in formal/lean" >&2
        exit 6
    fi
else
    echo "  (lean toolchain absent — structural grep only; install elan + run lake builds to reproduce)"
fi

echo "check-lean-t3-discharged: PASS (T1–T6 + v133 stack lemmas sorry-free)"
