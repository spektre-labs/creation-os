#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v66 σ-Silicon microbench — builds creation_os_v66 if needed, then runs
# the in-binary microbenchmark (INT8 GEMV, ternary GEMV, NTW decode, HSL).
set -euo pipefail
cd "$(dirname "$0")/../.."
if [[ ! -x ./creation_os_v66 ]]; then
    make -s standalone-v66
fi
echo "v66 σ-Silicon microbench:"
./creation_os_v66 --features | sed 's/^/  cpu features: /'
./creation_os_v66 --bench    | sed 's/^/  /'
