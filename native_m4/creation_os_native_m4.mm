/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Native M4 lab binary (opt-in): heterogeneous dispatch helpers.
 *
 * Scope: local-only demo helpers for Apple Silicon.
 * Non-goals: merge-gate, weight downloads, end-to-end LLM claims.
 */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "creation_os_native_m4.h"

#include <dispatch/dispatch.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#if defined(__APPLE__)
#include <unistd.h>
#endif

static int float_buffers_close(const float *a, const float *b, int n, float eps)
{
    for (int i = 0; i < n; i++) {
        float d = fabsf(a[i] - b[i]);
        if (d > eps)
            return 0;
    }
    return 1;
}

/* (popcount(byte) - 4) * scale is exact in float32 for scale=1/8 and pc in 0..8. */
static int float_buffers_exact(const float *a, const float *b, int n)
{
    return memcmp(a, b, (size_t)n * sizeof(float)) == 0;
}

static size_t cos_align_up(size_t n, size_t align)
{
    return (n + align - 1u) / align * align;
}

static void scalar_range(float *logits, const uint8_t *reputation, int begin, int end, float scale)
{
    if (!logits || !reputation || begin < 0 || end <= begin)
        return;
    for (int i = begin; i < end; i++) {
        int pc = __builtin_popcount((unsigned)reputation[i]);
        logits[i] += ((float)pc - 4.0f) * scale;
    }
}

/* Fast checks: tail loops, 64-byte block boundaries, sub-parallel vocab. */
static int self_test_edge_sizes(void)
{
    const float scale = 0.125f;
    static const int sizes[] = {1, 2, 7, 15, 16, 17, 63, 64, 65, 127, 128, 129, 1000, 4096, 65535};
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        int n = sizes[s];
        const size_t rep_sz = cos_align_up((size_t)n, 64);
        const size_t flt_sz = cos_align_up((size_t)n * sizeof(float), 64);
        uint8_t *rep = (uint8_t *)aligned_alloc(64, rep_sz);
        float *ref = (float *)aligned_alloc(64, flt_sz);
        float *neon = (float *)aligned_alloc(64, flt_sz);
        if (!rep || !ref || !neon) {
            fprintf(stderr, "FAIL alloc (edge n=%d)\n", n);
            free(rep);
            free(ref);
            free(neon);
            return 2;
        }
        for (int i = 0; i < n; i++) {
            rep[i] = (uint8_t)((i * 131U + 9U) & 0xFFU);
            ref[i] = 0.0f;
            neon[i] = 0.0f;
        }
        cos_living_weights_inplace(ref, rep, n, scale);
        cos_living_weights_neon_range(neon, rep, 0, n, scale);
        if (!float_buffers_exact(ref, neon, n)) {
            fprintf(stderr, "FAIL NEON vs scalar (edge n=%d)\n", n);
            free(rep);
            free(ref);
            free(neon);
            return 2;
        }
        free(rep);
        free(ref);
        free(neon);
    }
    return 0;
}

static int self_test_neon_range_slice(void)
{
    const int n = 512;
    const int begin = 37;
    const int end = 311;
    const float scale = 0.125f;
    const size_t rep_sz = cos_align_up((size_t)n, 64);
    const size_t flt_sz = cos_align_up((size_t)n * sizeof(float), 64);
    uint8_t *rep = (uint8_t *)aligned_alloc(64, rep_sz);
    float *ref = (float *)aligned_alloc(64, flt_sz);
    float *neon = (float *)aligned_alloc(64, flt_sz);
    if (!rep || !ref || !neon) {
        fprintf(stderr, "FAIL alloc (range slice)\n");
        free(rep);
        free(ref);
        free(neon);
        return 2;
    }
    for (int i = 0; i < n; i++) {
        rep[i] = (uint8_t)((i * 19U) & 0xFFU);
        ref[i] = 0.0f;
        neon[i] = 0.0f;
    }
    scalar_range(ref, rep, begin, end, scale);
    cos_living_weights_neon_range(neon, rep, begin, end, scale);
    if (!float_buffers_exact(ref, neon, n)) {
        fprintf(stderr, "FAIL NEON range slice [%d,%d)\n", begin, end);
        free(rep);
        free(ref);
        free(neon);
        return 2;
    }
    free(rep);
    free(ref);
    free(neon);
    return 0;
}

static int self_test(void)
{
    int e = self_test_edge_sizes();
    if (e != 0)
        return e;
    e = self_test_neon_range_slice();
    if (e != 0)
        return e;

    const int vocab = 65536; /* triggers NEON parallel path on Apple AArch64 */
    const float scale = 0.125f;
    const size_t rep_sz = cos_align_up((size_t)vocab, 64);
    const size_t flt_sz = cos_align_up((size_t)vocab * sizeof(float), 64);
    uint8_t *rep = (uint8_t *)aligned_alloc(64, rep_sz);
    float *base = (float *)aligned_alloc(64, flt_sz);
    float *neon = (float *)aligned_alloc(64, flt_sz);
    float *par = (float *)aligned_alloc(64, flt_sz);
    float *metal = (float *)aligned_alloc(64, flt_sz);
    if (!rep || !base || !neon || !par || !metal) {
        fprintf(stderr, "FAIL alloc\n");
        return 2;
    }

    for (int i = 0; i < vocab; i++) {
        rep[i] = (uint8_t)((i * 37) & 0xFF);
        base[i] = 0.0f;
        neon[i] = 0.0f;
        par[i] = 0.0f;
        metal[i] = 0.0f;
    }

    cos_living_weights_inplace(base, rep, vocab, scale);

    for (int i = 0; i < vocab; i++)
        neon[i] = 0.0f;
    cos_living_weights_neon_range(neon, rep, 0, vocab, scale);
    if (!float_buffers_exact(base, neon, vocab)) {
        fprintf(stderr, "FAIL NEON mismatch vs scalar\n");
        return 2;
    }

    for (int i = 0; i < vocab; i++)
        par[i] = 0.0f;
    cos_living_weights_neon_parallel(par, rep, vocab, scale);
    if (!float_buffers_exact(base, par, vocab)) {
        fprintf(stderr, "FAIL NEON parallel mismatch vs scalar\n");
        return 2;
    }

    memcpy(metal, neon, (size_t)vocab * sizeof(float));
#if defined(__APPLE__)
    /* Best-effort: compile metallib only if Xcode Metal toolchain is installed. */
    if (getenv("CREATION_OS_METALLIB") == NULL && access("native_m4/creation_os_lw.metallib", R_OK) != 0) {
        if (system("command -v xcrun >/dev/null 2>&1 && xcrun --find metal >/dev/null 2>&1 && ./native_m4/build_metallib.sh >/dev/null 2>&1") != 0) {
            fprintf(stderr, "native-m4: metallib build: SKIP (Metal toolchain not installed)\n");
        }
    }
#endif
    bool ok_metal = cos_living_weights_metal(metal, rep, vocab, scale);
    if (ok_metal) {
        if (!float_buffers_close(base, metal, vocab, 2e-3f)) {
            fprintf(stderr, "FAIL Metal mismatch vs scalar\n");
            return 2;
        }
        fprintf(stderr, "native-m4: Metal path: OK\n");
    } else {
        fprintf(stderr, "native-m4: Metal path: SKIP (no metallib or Metal unavailable)\n");
    }

    fprintf(stderr, "native-m4: SME runtime probe: %s\n", cos_runtime_has_sme() ? "yes" : "no");
    fprintf(stderr, "native-m4: self-test OK\n");
    free(rep);
    free(base);
    free(neon);
    free(par);
    free(metal);
    return 0;
}

static void bench_once(int vocab, int iters, int parallel)
{
    const size_t rep_sz = cos_align_up((size_t)vocab, 64);
    const size_t flt_sz = cos_align_up((size_t)vocab * sizeof(float), 64);
    uint8_t *rep = (uint8_t *)aligned_alloc(64, rep_sz);
    float *logits = (float *)aligned_alloc(64, flt_sz);
    if (!rep || !logits) {
        fprintf(stderr, "bench: alloc failed\n");
        free(rep);
        free(logits);
        return;
    }
    for (int i = 0; i < vocab; i++) {
        rep[i] = (uint8_t)(i & 0xFF);
        logits[i] = 0.0f;
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int k = 0; k < iters; k++) {
        /* reset each iter to keep workload stable */
        for (int i = 0; i < vocab; i++)
            logits[i] = 0.0f;
        if (parallel)
            cos_living_weights_neon_parallel(logits, rep, vocab, 0.125f);
        else
            cos_living_weights_neon_range(logits, rep, 0, vocab, 0.125f);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    double tok_per_s = (double)iters * (double)vocab / sec;
    fprintf(stderr, "bench: NEON%s vocab=%d iters=%d -> %.3e tokens/s (wall)\n",
            parallel ? "+GCD" : "", vocab, iters, tok_per_s);

    free(rep);
    free(logits);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && (!strcmp(argv[1], "--self-test") || !strcmp(argv[1], "--selftest")))
        return self_test();
    if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        printf("usage: %s [--self-test] [--bench [--vocab N] [--iters K] [--parallel]]\n", argv[0]);
        printf("  --bench requires positive --vocab and --iters (defaults: 65536, 200).\n");
        printf("\n");
        printf("Native M4 lab binary (opt-in). Not part of merge-gate.\n");
        printf("\n");
        printf("Optional:\n");
        printf("  CREATION_OS_METALLIB=path/to/creation_os_lw.metallib\n");
        printf("\n");
        printf("Build metallib (Darwin):\n");
        printf("  make metallib-m4\n");
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "--bench")) {
        int vocab = 1 << 16;
        int iters = 200;
        int parallel = 0;
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--vocab") && i + 1 < argc)
                vocab = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--iters") && i + 1 < argc)
                iters = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--parallel"))
                parallel = 1;
        }
        if (vocab <= 0 || iters <= 0) {
            fprintf(stderr, "bench: --vocab and --iters must be positive\n");
            return 2;
        }
        bench_once(vocab, iters, parallel);
        if (parallel == 0 && vocab >= 65536) {
            fprintf(stderr, "bench: also running parallel (--parallel) for large vocab...\n");
            bench_once(vocab, iters, 1);
        }
        return 0;
    }

    /* Demo: run living-weights on a perf queue. */
    const int vocab = 1024;
    float *logits = (float *)aligned_alloc(64, (size_t)vocab * sizeof(float));
    uint8_t *rep = (uint8_t *)aligned_alloc(64, (size_t)vocab);
    if (!logits || !rep) {
        fprintf(stderr, "alloc failed\n");
        free(logits);
        free(rep);
        return 2;
    }
    for (int i = 0; i < vocab; i++) {
        logits[i] = 0.0f;
        rep[i] = (uint8_t)(i & 0xFF);
    }

    dispatch_queue_t q = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
    dispatch_sync(q, ^{
      cos_living_weights_neon(logits, rep, vocab, 0.125f);
    });

    printf("native-m4: logits[0]=%.3f logits[1]=%.3f (demo)\n", logits[0], logits[1]);
    free(logits);
    free(rep);
    return 0;
}

