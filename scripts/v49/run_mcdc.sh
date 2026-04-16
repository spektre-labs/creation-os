#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v49: build + run MC/DC-oriented unit tests with gcov instrumentation (best-effort).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUTDIR="$ROOT/.build/v49-cov"
COVDIR="$ROOT/docs/v49/certification/coverage"
mkdir -p "$OUTDIR" "$COVDIR"

BIN="$OUTDIR/mcdc_test"
echo "=== v49 coverage build: $BIN ==="
cc -fprofile-arcs -ftest-coverage -O0 -Wall -std=c11 -I"$ROOT" \
  -o "$BIN" \
  "$ROOT/tests/v49/mcdc_tests.c" \
  "$ROOT/src/v49/sigma_gate.c" \
  "$ROOT/src/v47/sigma_kernel_verified.c" \
  "$ROOT/src/sigma/decompose.c" \
  -lm

"$BIN"

if command -v gcov >/dev/null 2>&1; then
  echo "=== gcov (text artifacts in $OUTDIR) ==="
  (
    cd "$OUTDIR"
    shopt -s nullglob 2>/dev/null || true
    for f in mcdc_test-*.gcno; do
      gcov -b "$f" >/dev/null 2>&1 || true
    done
  ) || true
  cp -f "$OUTDIR"/*.gcov "$COVDIR/" 2>/dev/null || true
  echo "Copied .gcov summaries (if any) to: $COVDIR"
else
  echo "gcov: SKIP (install Xcode CLI tools / gcc-coverage)"
fi

if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then
  echo "=== lcov + genhtml (optional) ==="
  lcov --capture --directory "$OUTDIR" --output-file "$OUTDIR/coverage.info" >/dev/null
  genhtml "$OUTDIR/coverage.info" --output-directory "$COVDIR/html" >/dev/null || true
  echo "HTML (if generated): $COVDIR/html/index.html"
else
  echo "lcov/genhtml: SKIP (optional HTML report)"
fi

echo "certify-coverage: OK"
