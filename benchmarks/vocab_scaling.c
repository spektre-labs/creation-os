/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Deterministic codebook stress: effective vocab size vs lookup + σ cost.
 * Does not require a multi-megabyte on-disk table; uses cos_codebook_row_fill modulo N.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/cos_bsc.h"
#include "src/tokenizer/cos_codebook_mmap.h"

int main(void)
{
    static const uint32_t sizes[] = {1024u, 8192u, 32768u, 131072u};
    const int iters = 200000;

    for (size_t si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
        uint32_t n = sizes[si];
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        uint64_t a[COS_W], b[COS_W];
        float acc = 0.f;
        for (int i = 0; i < iters; i++) {
            uint32_t ida = ((uint32_t)i * 1103515245u + 12345u) % n;
            uint32_t idb = ((uint32_t)i * 1664525u + 1013904223u) % n;
            cos_codebook_row_fill(ida, a);
            cos_codebook_row_fill(idb, b);
            acc += cos_hv_sigma(a, b);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double sec = (double)(t1.tv_sec - t0.tv_sec) + 1e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
        double ips = (double)iters / sec;
        printf("vocab_scaling: n=%7u iters=%d sigma_mean=%.6f lookups/sec=%.1f\n", n, iters, (double)(acc / (float)iters),
               ips);
    }
    return 0;
}
