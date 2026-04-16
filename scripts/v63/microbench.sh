#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v63 σ-Cipher microbenchmark.  Builds creation_os_v63 if absent, then
# runs the built-in ChaCha20-Poly1305 AEAD, BLAKE2b-256, X25519 scalar-
# multiplication, and sealed-envelope throughput measurements.
#
# No dependencies beyond make, clang, and libc.  Runs in ~2 s on M4.

set -euo pipefail

cd "$(dirname "$0")/../.."

if [[ ! -x ./creation_os_v63 ]]; then
    make -s standalone-v63
fi

echo "v63 σ-Cipher microbench:"
./creation_os_v63 --bench | sed 's/^/  /'
