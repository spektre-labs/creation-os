#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v64 σ-Intellect microbench runner.  Builds creation_os_v64 if needed
# and emits the canonical perf report (MCTS-σ / skill retrieve /
# tool-authz / MoD-σ).  Never fails on throughput numbers; it fails
# only if the build itself fails.

set -euo pipefail
cd "$(dirname "$0")/../.."

if [[ ! -x ./creation_os_v64 ]]; then
    make -s standalone-v64
fi

echo "v64 σ-Intellect microbench:"
./creation_os_v64 --bench | sed 's/^/  /'
