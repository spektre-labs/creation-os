/* SPDX-License-Identifier: AGPL-3.0-or-later
 * AGI-style gate in HV space: K-agent parliament + NEON memory bank argmin.
 * Evidence: Measured (microbench) — archive host metadata when citing rates.
 */
#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../core/cos_neon_retrieval.h"
#include "../core/cos_parliament.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifndef N_MEM
#define N_MEM 2048
#endif
#ifndef N_ROUNDS
#define N_ROUNDS 500
#endif
#define K_AGENTS 7

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
    uint64_t rng = 0xA6000001ULL;
    uint64_t agents[K_AGENTS * COS_W];
    uint64_t consensus[COS_W];
    uint64_t mem[N_MEM * COS_W];

    for (int k = 0; k < K_AGENTS; k++)
        cos_hv_random(&rng, agents + (size_t)k * (size_t)COS_W);
    cos_hv_bit_parliament(consensus, agents, K_AGENTS);

    for (int r = 0; r < N_MEM; r++)
        cos_hv_random(&rng, mem + (size_t)r * (size_t)COS_W);
    /* Plant row 0 identical to consensus so argmin must be 0. */
    for (int w = 0; w < COS_W; w++)
        mem[w] = consensus[w];

    int hit = cos_hv_argmin_hamming_rows_hw(consensus, mem, N_MEM);
    if (hit != 0) {
        printf("hv_agi_gate_neon: FAIL argmin expected 0 got %d\n", hit);
        return 1;
    }

    double t0 = mono_sec();
    volatile int sink = 0;
    for (int round = 0; round < N_ROUNDS; round++) {
        sink += cos_hv_argmin_hamming_rows_hw(consensus, mem, N_MEM);
        (void)sink;
    }
    double sec = mono_sec() - t0;

    printf("hv_agi_gate_neon: K=%d n_mem=%d rounds=%d parliament+argmin_wall_s=%.6f rounds_per_sec=%.0f\n", K_AGENTS,
           N_MEM, N_ROUNDS, sec, sec > 1e-12 ? (double)N_ROUNDS / sec : 0.0);
#if defined(__aarch64__)
    printf("hv_agi_gate_neon: hamming_path=AArch64_NEON\n");
#else
    printf("hv_agi_gate_neon: hamming_path=scalar_fallback\n");
#endif
    return 0;
}
