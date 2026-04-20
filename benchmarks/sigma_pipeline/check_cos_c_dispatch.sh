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

# ---- D. structural: cos must carry the C-first dispatcher and
#        MUST NOT re-introduce the Python-subshell fallback.  After
#        CLOSE-4 the only dispatch helpers are exec_sibling() /
#        prefer_c_or_hint() / exec_preferred() — run_py_module and
#        its sh_quote helper were retired.
if ! grep -q 'exec_sibling' cli/cos.c; then
    echo "FAIL cli/cos.c no longer contains exec_sibling dispatcher" >&2
    exit 6
fi
if ! grep -q 'prefer_c_or_hint\|prefer_c_else_py' cli/cos.c; then
    echo "FAIL cli/cos.c missing C-first dispatcher helper" >&2
    exit 6
fi
if grep -q 'run_py_module' cli/cos.c; then
    echo "FAIL cli/cos.c re-introduced run_py_module (must stay pure-C)" >&2
    exit 7
fi

# ---- E. front door must also route the seven long-tail kernels
#        (CLOSE-4): mcp / a2a / team / lora / voice / offline /
#        watchdog / index.  We assert the dispatch lines are
#        present in cli/cos.c and that `cos help` advertises each
#        one.  Capture the help once into a variable so the grep
#        loop does not race the help producer (set -o pipefail
#        would otherwise report SIGPIPE from the help side when
#        `grep -q` exits early).
HELP="$(NO_COLOR=1 ./cos help 2>&1 || true)"
for verb in mcp a2a team lora voice offline watchdog index; do
    if ! grep -qE "strcmp\(argv\[1\], \"$verb\"\) *== *0" cli/cos.c; then
        echo "FAIL cli/cos.c missing dispatch line for 'cos $verb'" >&2
        exit 8
    fi
    if ! printf '%s\n' "$HELP" | grep -qE "^  $verb( |$)"; then
        echo "FAIL 'cos help' missing entry for '$verb'" >&2
        exit 9
    fi
done
echo "  cos {mcp,a2a,team,lora,voice,offline,watchdog,index} dispatched in pure C"

echo "check-cos-c-dispatch: PASS (front door routes chat/benchmark/cost + 8 long-tail kernels to C siblings; zero Python)"
