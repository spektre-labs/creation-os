#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v58 σ-Cache deterministic microbench wrapper.
#
# Runs ./creation_os_v58 --microbench across a small parameter sweep
# so the per-token decision throughput is reported at three context
# lengths (1024, 4096, 16384 tokens) on whatever host runs `make
# microbench-v58`.  The bench is purely synthetic — it does not load
# a model, open sockets, or download anything.  It exists to give an
# honest, reproducible kernel timing on the local hardware so users
# can spot pathological regressions.
#
# Exit codes:
#   0 — every sweep iteration ran cleanly
#   1 — at least one iteration exited non-zero
#   2 — the binary is missing (run `make standalone-v58` first)

set -u

BIN="./creation_os_v58"
if [[ ! -x "$BIN" ]]; then
    echo "microbench-v58: $BIN not found — run 'make standalone-v58' first"
    exit 2
fi

SWEEPS=(
    "1024 200"
    "4096 100"
    "16384 50"
)

cat <<'EOF'

Creation OS v58 — σ-Cache microbench
====================================

Deterministic synthetic-workload kernel timing.
No model.  No network.  Same numbers across runs on the same host.

EOF

fail=0
for entry in "${SWEEPS[@]}"; do
    n_tokens="${entry%% *}"
    n_iters="${entry##* }"
    echo "----- N=${n_tokens} iters=${n_iters} -----"
    if ! "$BIN" --microbench "$n_tokens" "$n_iters"; then
        echo "microbench-v58: FAIL (n=${n_tokens} iters=${n_iters})"
        fail=1
    fi
    echo
done

if [[ $fail -ne 0 ]]; then
    echo "microbench-v58: FAIL (one or more sweeps non-zero)"
    exit 1
fi
echo "microbench-v58: OK (3-point sweep completed; deterministic)"
exit 0
