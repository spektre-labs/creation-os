#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v62 — Reasoning Fabric microbench harness.
#
# Builds creation_os_v62 (idempotent) and runs its bench harness for
# NSAttn attention + EBT minimization throughput on this host.
set -euo pipefail
cd "$(dirname "$0")/../.."
make -s standalone-v62
echo "v62 microbench (host: $(uname -m), $(uname -s)):"
./creation_os_v62 --bench
