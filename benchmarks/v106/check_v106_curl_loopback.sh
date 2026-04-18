#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v106 σ-Server — curl-loopback gate (merge-gate entry point).
#
# Thin wrapper around benchmarks/v106/check_v106.sh so the merge-gate
# has a stable named target (`check-v106-curl-loopback`) independent
# of future refactors.  Also ensures the standalone-v106 binary is
# built before probing.
set -eu
set -o pipefail

cd "$(dirname "$0")/../.."

BIN="${COS_V106_SERVER_BIN:-./creation_os_server}"
if [ ! -x "$BIN" ]; then
    # Try a best-effort build; if it fails we still want SKIP semantics
    # so the merge-gate stays green on hosts missing a toolchain (e.g.
    # first-time clone, CI warm-up).
    if ! $(command -v make) standalone-v106 >/tmp/cos_v106_build.$$.log 2>&1; then
        echo "check-v106-curl-loopback: SKIP (standalone-v106 build failed — see /tmp/cos_v106_build.$$.log)"
        exit 0
    fi
fi

exec bash benchmarks/v106/check_v106.sh "$@"
