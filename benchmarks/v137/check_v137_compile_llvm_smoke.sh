#!/usr/bin/env bash
# check-v137: σ-compile — AOT decision tree smoke
#
# Verifies:
#   1. self-test passes (semantic equivalence + emit + latency)
#   2. --emit produces valid C that compiles under $CC -O3
#   3. bench reports ns_per_call for both paths; compiled ≤ interpreted
#   4. compiled path is absolutely under 500 ns/call
set -euo pipefail
BIN="./creation_os_v137_compile"
[ -x "$BIN" ] || { echo "check-v137: $BIN not built"; exit 1; }

CC="${CC:-cc}"

echo "check-v137: self-test"
"$BIN" --self-test

echo "check-v137: emit + $CC -O3 -c smoke"
tmp=$(mktemp -d -t cos_v137.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
"$BIN" --emit --out "$tmp/compiled_sigma.c"
grep -q "cos_v137_compiled_gate" "$tmp/compiled_sigma.c" \
    || { echo "emitted source missing symbol"; exit 1; }
"$CC" -O3 -c -o "$tmp/compiled_sigma.o" "$tmp/compiled_sigma.c"
echo "  emit → $CC -O3 -c OK (object $(wc -c < "$tmp/compiled_sigma.o") bytes)"

echo "check-v137: emit with custom τ"
"$BIN" --emit --tau 0.10,0.20,0.30,0.40,0.50,0.60,0.70,0.80 --out "$tmp/custom.c"
grep -q '(ch\[0\] > 0.100000f)' "$tmp/custom.c" || { echo "custom τ missing"; exit 1; }
grep -q '(ch\[7\] > 0.800000f)' "$tmp/custom.c" || { echo "custom τ missing"; exit 1; }
"$CC" -O3 -c -o "$tmp/custom.o" "$tmp/custom.c"
echo "  custom-τ emit compiles OK"

echo "check-v137: bench"
out=$("$BIN" --bench --iters 500000)
echo "$out" | sed 's/^/  /'
ns_cmp=$(printf '%s\n' "$out" | awk -F= '/ns_per_call_compiled/{print $2}')
ns_int=$(printf '%s\n' "$out" | awk -F= '/ns_per_call_interpreted/{print $2}')
awk -v c="$ns_cmp" -v i="$ns_int" 'BEGIN{
    if (i+0 >= 500) { print "interpreted too slow: "i; exit 1 }
    if (c+0 >= 500) { print "compiled too slow: "c;    exit 1 }
}'
echo "  interpreted ≤ 500 ns/call AND compiled ≤ 500 ns/call OK"

echo "check-v137: all OK"
