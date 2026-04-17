#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v67 σ-Noesis microbench — builds creation_os_v67 if needed, then runs
# the in-binary microbench (top-K, BM25, dense-sig, beam, NBL).
set -euo pipefail
cd "$(dirname "$0")/../.."
if [[ ! -x ./creation_os_v67 ]]; then
    make -s standalone-v67
fi
echo "v67 σ-Noesis microbench:"
./creation_os_v67 --bench | sed 's/^/  /'
