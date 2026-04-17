#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v69 σ-Constellation microbenchmark runner.
#
# Builds creation_os_v69 (if not already built) and runs the
# --bench driver.  Prints a banner with the host compiler + arch so
# the throughput numbers are self-contained.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x creation_os_v69 ]]; then
    echo "v69 microbench: building creation_os_v69..."
    make -s standalone-v69
fi

echo "=============================================================="
echo "  v69 σ-Constellation microbench"
echo "  host: $(uname -mrs)"
echo "  cc:   $({ ${CC:-cc} --version 2>/dev/null || true; } | head -n1)"
echo "=============================================================="
./creation_os_v69 --bench
