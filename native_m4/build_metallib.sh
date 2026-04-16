#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "build_metallib: SKIP (Darwin only)" >&2
  exit 0
fi

mkdir -p native_m4

AIR="native_m4/creation_os_lw.air"
METALLIB="native_m4/creation_os_lw.metallib"

xcrun -sdk macosx metal -c native_m4/creation_os_living_weights.metal -o "$AIR"
xcrun -sdk macosx metallib "$AIR" -o "$METALLIB"

echo "build_metallib: OK -> $METALLIB"
