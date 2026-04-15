/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Same benchmark as bench/gemm_vs_bsc.c (kept under core/ per tree spec).
 */
#if !defined(_WIN32) && !defined(__APPLE__)
#define _POSIX_C_SOURCE 200809L
#endif
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define D 4096
#define W 64

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

int cos_gemm_bench_main(void)
{
    static float fa[D], fb[D];
    static uint64_t ba[W], bb[W];
    uint64_t rng = (uint64_t)time(NULL);
    for (int i = 0; i < D; i++) {
        rng = rng * 6364136223846793005ULL + 1;
        fa[i] = (float)(rng % 1000u) / 500.0f - 1.0f;
        fb[i] = (float)((rng >> 32) % 1000u) / 500.0f - 1.0f;
    }
    for (int i = 0; i < W; i++) {
        rng = rng * 6364136223846793005ULL + 1;
        ba[i] = rng;
        bb[i] = rng * 6364136223846793005ULL + 1;
    }
    int trials = 100000;
    double t0 = mono_sec();
    volatile float gr = 0;
    for (int t = 0; t < trials; t++) {
        float dot = 0, na = 0, nb = 0;
        for (int i = 0; i < D; i++) {
            dot += fa[i] * fb[i];
            na += fa[i] * fa[i];
            nb += fb[i] * fb[i];
        }
        gr = dot / (sqrtf(na) * sqrtf(nb) + 1e-8f);
    }
    double gemm_sec = mono_sec() - t0;

    t0 = mono_sec();
    volatile float br = 0;
    for (int t = 0; t < trials; t++) {
        uint32_t d = 0;
        for (int i = 0; i < W; i++)
            d += (uint32_t)__builtin_popcountll(ba[i] ^ bb[i]);
        float r = (float)d / (float)D;
        br = r * r;
    }
    double bsc_sec = mono_sec() - t0;

    printf("core/gemm_vs_bsc: trials=%d D=%d\n", trials, D);
    printf("GEMM wall_s   %.9f\n", gemm_sec);
    printf("BSC  wall_s   %.9f\n", bsc_sec);
    if (bsc_sec > 1e-12 && gemm_sec > 1e-12)
        printf("wall_clock_ratio GEMM/BSC: %.2f\n", gemm_sec / bsc_sec);
    printf("memory_bytes_vector GEMM %d BSC %d ratio %d\n", D * 4, D / 8, (D * 4) / (D / 8));
    printf("op_proxy_per_trial GEMM %d BSC %d ratio %d\n", D * 6, W * 2, (D * 6) / (W * 2));
    if (gemm_sec > 1e-12 && bsc_sec > 1e-12) {
        double gemm_tps = (double)trials / gemm_sec;
        double bsc_tps = (double)trials / bsc_sec;
        printf("trials_per_sec_GEMM %.0f\n", gemm_tps);
        printf("trials_per_sec_BSC  %.0f\n", bsc_tps);
        printf("trials_per_sec_ratio_BSC_over_GEMM %.1f\n", bsc_tps / gemm_tps);
    }
    (void)gr;
    (void)br;
    return 0;
}
