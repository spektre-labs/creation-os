#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — lattice-check microbench.  Rebuilds a small
# standalone harness with optimised flags and runs a deterministic
# timing test at three batch sizes.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

CC="${CC:-cc}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat >"$TMP/bench.c" <<'EOF'
#include "citadel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double elapsed(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec - a.tv_sec) +
           (double)(b.tv_nsec - a.tv_nsec) / 1e9;
}

int main(int argc, char **argv) {
    int n = (argc > 1) ? atoi(argv[1]) : 10000;
    if (n <= 0) n = 10000;
    cos_v61_label_t *s = aligned_alloc(64, ((size_t)n * sizeof(cos_v61_label_t) + 63) & ~63);
    cos_v61_label_t *o = aligned_alloc(64, ((size_t)n * sizeof(cos_v61_label_t) + 63) & ~63);
    uint8_t *ops = aligned_alloc(64, ((size_t)n + 63) & ~63);
    uint8_t *rs  = aligned_alloc(64, ((size_t)n + 63) & ~63);
    uint32_t rng = 0xC0DECAFE;
    for (int i = 0; i < n; ++i) {
        rng = rng * 1664525u + 1013904223u;
        s[i].clearance = (uint8_t)(rng >> 24);
        s[i].integrity = (uint8_t)(rng >> 16);
        s[i].compartments = (uint16_t)(rng & 0xFFFF);
        s[i]._reserved = 0;
        rng = rng * 1664525u + 1013904223u;
        o[i].clearance = (uint8_t)(rng >> 24);
        o[i].integrity = (uint8_t)(rng >> 16);
        o[i].compartments = (uint16_t)(rng & 0xFFFF);
        o[i]._reserved = 0;
        ops[i] = (uint8_t)(1 + (i % 3));
    }
    struct timespec t0, t1;
    const int laps = 200;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int j = 0; j < laps; ++j) {
        cos_v61_lattice_check_batch(s, o, ops, n, rs);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double dt = elapsed(t0, t1);
    double ops_s = (double)n * (double)laps / dt;
    printf("v61 lattice_check_batch n=%d: %.4fs / %d laps = %.2e decisions/s\n",
           n, dt, laps, ops_s);
    free(s); free(o); free(ops); free(rs);
    return 0;
}
EOF

"$CC" -O2 -std=c11 -Wall -Isrc/v61 \
  -o "$TMP/bench" "$TMP/bench.c" src/v61/citadel.c -lm

for N in 1024 16384 262144; do
  "$TMP/bench" "$N"
done

echo "microbench-v61: OK"
