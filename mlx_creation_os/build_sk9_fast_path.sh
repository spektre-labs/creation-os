#!/usr/bin/env bash
# Build libsk9_fast_path.dylib from sk9_fast_path.asm (aarch64 only).
set -euo pipefail
cd "$(dirname "$0")"
if [[ "$(uname -m)" != "arm64" ]]; then
  echo "sk9_fast_path: arm64 required (got $(uname -m))" >&2
  exit 1
fi
clang -arch arm64 -O2 -dynamiclib -o libsk9_fast_path.dylib sk9_fast_path.asm
echo "built $(pwd)/libsk9_fast_path.dylib"
