#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v70 σ-Hyperscale microbenchmark runner.
#
# Builds creation_os_v70 (if not already built) and runs the
# --bench driver.  Prints a banner with the host compiler + arch so
# the throughput numbers are self-contained.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x creation_os_v70 ]]; then
    echo "v70 microbench: building creation_os_v70..."
    make -s standalone-v70
fi

echo "=============================================================="
echo "  v70 σ-Hyperscale microbench"
echo "  host: $(uname -mrs)"
echo "  cc:   $({ ${CC:-cc} --version 2>/dev/null || true; } | head -n1)"
echo "=============================================================="
./creation_os_v70 --bench
