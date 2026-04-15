/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Batch coherence (Hamming) microbench — edge / robotics style gate loop.
 * Uses cos_hv_hamming_hw (NEON on AArch64, scalar elsewhere).
 */
#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../core/cos_neon_hamming.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static double mono_sec(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#else
    return (double)clock() / (double)CLOCKS_PER_SEC;
#endif
}

int main(void)
{
    uint64_t rng = 0xDEADBEEFULL;
    uint64_t a[COS_W], b[COS_W];
    cos_hv_random(&rng, a);
    cos_hv_random(&rng, b);

    uint32_t h0 = cos_hv_hamming(a, b);
    uint32_t h1 = cos_hv_hamming_hw(a, b);
    if (h0 != h1) {
        printf("coherence_gate_batch: FAIL hamming scalar=%u hw=%u\n", h0, h1);
        return 1;
    }

    const int trials = 200000;
    double t0 = mono_sec();
    volatile uint32_t sink = 0;
    for (int t = 0; t < trials; t++)
        sink += cos_hv_hamming_hw(a, b);
    double sec = mono_sec() - t0;
    (void)sink;

    printf("coherence_gate_batch: trials=%d D=%d hamming_checks/sec=%.0f\n", trials, COS_D,
           sec > 1e-12 ? (double)trials / sec : 0.0);
#if defined(__aarch64__)
    printf("coherence_gate_batch: hw_path=AArch64_NEON\n");
#else
    printf("coherence_gate_batch: hw_path=scalar_fallback\n");
#endif
    return 0;
}
