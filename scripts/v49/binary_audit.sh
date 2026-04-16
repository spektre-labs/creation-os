#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v49: “defense-in-depth” binary / toolchain hygiene checks (NOT a substitute for
# independent certification authority review).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BINARY="${BINARY:-./creation_os_v47}"
AUDIT="${AUDIT_DIR:-$ROOT/.build/v49-audit}"
mkdir -p "$AUDIT"

echo "=== v49 binary audit (lab hygiene) ==="
echo "BINARY=$BINARY"

if [[ ! -f "$BINARY" ]]; then
  echo "SKIP: binary missing (try: make standalone-v47 && BINARY=./creation_os_v47 $0)"
  exit 0
fi

echo "=== nm (symbol inventory) ==="
nm "$BINARY" 2>/dev/null | LC_ALL=C sort > "$AUDIT/symbols.txt" || true
echo "symbols: $(wc -l < "$AUDIT/symbols.txt" | tr -d ' ') lines -> $AUDIT/symbols.txt"

echo "=== strings (narrow secret grep — expect occasional false positives) ==="
set +e
strings "$BINARY" | LC_ALL=C grep -Ei 'api_key|bearer [a-z0-9]{20,}|sk-[a-z0-9]{10,}' > "$AUDIT/secrets_hits.txt"
SE=$?
set -e
if [[ $SE -eq 0 && -s "$AUDIT/secrets_hits.txt" ]]; then
  echo "WARN: pattern hits (manual triage required):"
  head -n 20 "$AUDIT/secrets_hits.txt"
else
  echo "PASS: no obvious high-signal secret patterns in strings output"
fi

echo "=== reproducible object compare (sigma_kernel_verified.c) ==="
OBJ1="$AUDIT/sigma_kernel_verified.1.o"
OBJ2="$AUDIT/sigma_kernel_verified.2.o"
cc -O2 -Wall -std=c11 -I"$ROOT" -c "$ROOT/src/v47/sigma_kernel_verified.c" -o "$OBJ1"
cc -O2 -Wall -std=c11 -I"$ROOT" -c "$ROOT/src/v47/sigma_kernel_verified.c" -o "$OBJ2"
if cmp -s "$OBJ1" "$OBJ2"; then
  echo "PASS: two consecutive compiles produced identical .o"
else
  echo "WARN: object files differ (timestamps embedded? toolchain nondeterminism?)"
fi

echo "=== strict warnings compile (single TU) ==="
cc -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -O2 -std=c11 -I"$ROOT" -c "$ROOT/src/v47/sigma_kernel_verified.c" -o "$AUDIT/sigma_kernel_verified.strict.o"
echo "PASS: strict compile of sigma_kernel_verified.c"

echo "=== ASan+UBSan smoke (mcdc test binary) ==="
ASAN_BIN="$AUDIT/mcdc_asan"
cc -fsanitize=address,undefined -O1 -g -Wall -std=c11 -I"$ROOT" -o "$ASAN_BIN" \
  "$ROOT/tests/v49/mcdc_tests.c" \
  "$ROOT/src/v49/sigma_gate.c" \
  "$ROOT/src/v47/sigma_kernel_verified.c" \
  "$ROOT/src/sigma/decompose.c" \
  -lm
"$ASAN_BIN"
echo "PASS: ASan+UBSan mcdc binary"

if command -v valgrind >/dev/null 2>&1; then
  echo "=== valgrind (lab binary self-test) ==="
  VAL_BIN="${VALGRIND_BINARY:-./creation_os_v47}"
  if [[ -x "$VAL_BIN" ]]; then
    valgrind --leak-check=full --error-exitcode=1 "$VAL_BIN" --self-test
    echo "PASS: valgrind clean on $VAL_BIN --self-test"
  else
    echo "valgrind: SKIP ($VAL_BIN not executable)"
  fi
else
  echo "valgrind: SKIP"
fi

if command -v checksec >/dev/null 2>&1; then
  checksec --file="$BINARY" | tee "$AUDIT/checksec.txt" >/dev/null || true
  echo "checksec: wrote $AUDIT/checksec.txt"
else
  echo "checksec: SKIP"
fi

echo ""
echo "=== AUDIT COMPLETE (lab) ==="
echo "Artifacts: $AUDIT"
echo "NOTE: objdump-based syscall / networking forensics are intentionally omitted here (high false-positive rate)."
echo "NOTE: do not infer “zero networking” from this script alone — use OS-level sandboxing + independent review for real deployments."
