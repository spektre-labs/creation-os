#!/usr/bin/env bash
#
# Smoke test for FIX-3: cos front-door C-first dispatch.
#
# When the native sibling binary exists (cos-chat, cos-benchmark,
# cos-cost), the `cos` launcher must prefer it over the Python
# reference implementation so the front door has zero runtime
# dependency on Python.  Python remains the fallback for pre-build
# smoke tests on fresh checkouts.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

for bin in cos cos-chat cos-benchmark cos-cost; do
    [[ -x "./$bin" ]] || { echo "missing ./$bin — run 'make $bin'" >&2; exit 2; }
done

# ---- A. banner round-trip: cos chat --banner-only must match
#       cos-chat --banner-only byte-for-byte (proves C dispatch).
FRONT="$(./cos chat --banner-only)"
NATIVE="$(./cos-chat --banner-only)"
if [[ "$FRONT" != "$NATIVE" ]]; then
    echo "FAIL cos chat does not match cos-chat directly:" >&2
    diff <(printf '%s\n' "$FRONT") <(printf '%s\n' "$NATIVE") >&2 || true
    exit 3
fi
echo "  cos chat --banner-only  == cos-chat --banner-only"

# ---- B. cos benchmark --json must match cos-benchmark --json.
F_BM="$(./cos benchmark --json)"
N_BM="$(./cos-benchmark --json)"
if [[ "$F_BM" != "$N_BM" ]]; then
    echo "FAIL cos benchmark does not match cos-benchmark --json" >&2
    exit 4
fi
echo "  cos benchmark --json    == cos-benchmark --json"

# ---- C. cos cost --json must match cos-cost --json.
F_CO="$(./cos cost --json)"
N_CO="$(./cos-cost --json)"
if [[ "$F_CO" != "$N_CO" ]]; then
    echo "FAIL cos cost does not match cos-cost --json" >&2
    exit 5
fi
echo "  cos cost --json         == cos-cost --json"

# ---- D. structural: cos must mention prefer_c_else_py + exec_sibling,
#        proving the C-first dispatcher is compiled in (guard against
#        accidental reversion to pure-Python dispatch).
if ! grep -q 'prefer_c_else_py' cli/cos.c; then
    echo "FAIL cli/cos.c no longer contains prefer_c_else_py dispatcher" >&2
    exit 6
fi

echo "check-cos-c-dispatch: PASS (front door routes chat/benchmark/cost to C siblings)"
