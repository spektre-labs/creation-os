#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Fat Mach-O: arm64 + x86_64 slices for `cos` → ./cos-universal
# Requires Darwin + Apple clang. CFLAGS omit -march=native (incompatible with -arch).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [ "$(uname -s)" != Darwin ]; then
  echo "cos-universal: SKIP (Darwin only)" >&2
  exit 0
fi

CC_BIN="${CC:-clang}"
SLICE_CFLAGS="-O2 -Wall -std=c11 -D_GNU_SOURCE -D_DARWIN_C_SOURCE"

rm -f cos cos.arm64 cos.x86_64 cos-universal cos-demo.universal

echo "cos-universal: arm64 slice..."
make cos CC="${CC_BIN}" CFLAGS="${SLICE_CFLAGS} -arch arm64"
mv cos cos.arm64

echo "cos-universal: x86_64 slice..."
make cos CC="${CC_BIN}" CFLAGS="${SLICE_CFLAGS} -arch x86_64"
mv cos cos.x86_64

lipo -create -output cos-universal cos.arm64 cos.x86_64
rm -f cos.arm64 cos.x86_64
ln -f cos-universal cos-demo
echo "cos-universal: OK -> ./cos-universal (cos-demo -> same binary)"
