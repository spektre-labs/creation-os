#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — Wasmtime sandbox harness.
#
# Compiles wasm/example_tool.c to wasm32-wasi if a WASI-SDK clang is
# available, then runs it under wasmtime with ambient authority
# disabled.  Honest SKIP if toolchain is missing; no silent PASS.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

# Find a WASI-capable clang.  First env var, then PATH fallbacks.
WASI_CC="${WASI_CC:-}"
if [ -z "$WASI_CC" ]; then
  for cand in \
    /opt/wasi-sdk/bin/clang \
    /usr/local/opt/wasi-sdk/bin/clang \
    "$HOME/wasi-sdk/bin/clang" \
    wasi-clang; do
    if command -v "$cand" >/dev/null 2>&1; then
      WASI_CC="$cand"
      break
    fi
  done
fi

if [ -z "$WASI_CC" ] || ! command -v "$WASI_CC" >/dev/null 2>&1; then
  echo "wasm-sandbox: SKIP (no WASI clang on PATH; set WASI_CC= to a wasi-sdk clang)"
  exit 0
fi

if ! command -v wasmtime >/dev/null 2>&1; then
  echo "wasm-sandbox: SKIP (wasmtime not on PATH)"
  exit 0
fi

echo "wasm-sandbox: using $WASI_CC + $(wasmtime --version)"

OUT=".build/wasm/example_tool.wasm"
mkdir -p "$(dirname "$OUT")"

"$WASI_CC" -O2 -Wall -o "$OUT" wasm/example_tool.c
echo "wasm-sandbox: compiled $OUT"

# Run with the tightest defaults we can: no cache, no dir mount, and
# strictly one-shot execution.  Tools in production would receive an
# explicit --dir= only for paths σ-Shield authorised.
OUTPUT="$(wasmtime run --disable-cache "$OUT" "σ-Citadel WASM sandbox OK" 2>&1 || true)"
echo "wasm-sandbox: tool output: $OUTPUT"

if printf '%s' "$OUTPUT" | grep -q "σ-Citadel WASM sandbox OK"; then
  echo "wasm-sandbox: OK (tool executed inside wasmtime with ambient authority disabled)"
else
  echo "wasm-sandbox: FAIL (unexpected tool output)"
  exit 1
fi
