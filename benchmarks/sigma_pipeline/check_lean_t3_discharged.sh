#!/usr/bin/env bash
#
# FIX-7: structural check that T3 gate-monotonicity (and three
# sibling gate-order theorems) have a Lean 4 discharge without
# `sorry`, over the abstract `LinearOrder α` setting.
#
# When a Lean 4 toolchain is available (lean / lake on PATH), we
# additionally invoke `lake env lean` to confirm the file type-checks
# cleanly.  Absent the toolchain, the structural grep is the
# strongest local evidence — it guarantees the PROOF TEXT is shipped
# in the tree and that no committer reverted it to a `sorry`.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

LEAN="hw/formal/v259/Measurement.lean"
[[ -f "$LEAN" ]] || { echo "missing $LEAN" >&2; exit 2; }

# ---- 1. four abstract-order discharges must be present --------------------
for thm in gateα_totality gateα_boundary_tiebreak \
           gateα_monotone_in_sigma gateα_anti_monotone_in_tau; do
    grep -q "^theorem $thm" "$LEAN" \
        || { echo "FAIL: missing theorem $thm in $LEAN" >&2; exit 3; }
done

# ---- 2. none of the four abstract-order bodies may contain `sorry` -------
#        We match each theorem block up to the next `theorem` header or
#        end-of-file and require the block is `sorry`-free.
awk -v RS='(^|\n)theorem ' '
    NR == 1 { next }    # everything before the first theorem
    /^gateα_(totality|boundary_tiebreak|monotone_in_sigma|anti_monotone_in_tau)/ {
        if (index($0, " sorry") > 0 || index($0, "\nsorry") > 0) {
            thm = $1; sub(/[^A-Za-z_α0-9].*/, "", thm)
            print "FAIL: theorem " thm " ends in `sorry`" > "/dev/stderr"
            exit 4
        }
    }
' "$LEAN"

# ---- 3. companion docs line must match ------------------------------------
grep -q "DISCHARGED over .LinearOrder α. ..gateα_monotone_in_sigma.)" \
    docs/v259/README.md \
    || { echo "FAIL: docs/v259/README.md missing T3α ledger line" >&2; exit 5; }

# ---- 4. optional: run Lean when a toolchain is on PATH --------------------
if command -v lake >/dev/null 2>&1 && command -v lean >/dev/null 2>&1; then
    echo "  (lean toolchain found — invoking lake env lean)"
    if lake env lean "$LEAN" >/tmp/cos_lean.out 2>&1; then
        echo "  lean: file type-checks cleanly"
    else
        tail -20 /tmp/cos_lean.out >&2 || true
        echo "FAIL: lake env lean $LEAN returned non-zero" >&2
        exit 6
    fi
else
    echo "  (lean toolchain absent — structural grep only; add 'make formal-v259' when lake is wired)"
fi

echo "check-lean-t3-discharged: PASS (gateα_{totality,boundary,monotone,anti-monotone} all sorry-free)"
