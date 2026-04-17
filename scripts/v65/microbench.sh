#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v65 σ-Hypercortex microbench — builds creation_os_v65 if needed,
# then runs the in-binary microbenchmark (Hamming, bind, cleanup, HVL).
set -euo pipefail
cd "$(dirname "$0")/../.."
if [[ ! -x ./creation_os_v65 ]]; then
    make -s standalone-v65
fi
echo "v65 σ-Hypercortex microbench:"
./creation_os_v65 --bench | sed 's/^/  /'
