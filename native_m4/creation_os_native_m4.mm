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

static int self_test(void)
{
    const int vocab = 256; /* multiple of 16 for NEON fast path */
    const float scale = 0.125f;
    uint8_t *rep = (uint8_t *)aligned_alloc(64, (size_t)vocab);
    float *base = (float *)aligned_alloc(64, (size_t)vocab * sizeof(float));
    float *neon = (float *)aligned_alloc(64, (size_t)vocab * sizeof(float));
    float *metal = (float *)aligned_alloc(64, (size_t)vocab * sizeof(float));
    if (!rep || !base || !neon || !metal) {
        fprintf(stderr, "FAIL alloc\n");
        return 2;
    }

    for (int i = 0; i < vocab; i++) {
        rep[i] = (uint8_t)((i * 37) & 0xFF);
        base[i] = 0.0f;
        neon[i] = 0.0f;
        metal[i] = 0.0f;
    }

    memcpy(neon, base, (size_t)vocab * sizeof(float));
    cos_living_weights_inplace(base, rep, vocab, scale);
    cos_living_weights_neon(neon, rep, vocab, scale);
    if (!float_buffers_close(base, neon, vocab, 1e-4f)) {
        fprintf(stderr, "FAIL NEON mismatch vs scalar\n");
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
    free(metal);
    return 0;
}

static void bench_once(int vocab, int iters)
{
    uint8_t *rep = (uint8_t *)aligned_alloc(64, (size_t)vocab);
    float *logits = (float *)aligned_alloc(64, (size_t)vocab * sizeof(float));
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
        cos_living_weights_neon(logits, rep, vocab, 0.125f);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    double tok_per_s = (double)iters * (double)vocab / sec;
    fprintf(stderr, "bench: NEON vocab=%d iters=%d -> %.3e tokens/s (wall)\n", vocab, iters, tok_per_s);

    free(rep);
    free(logits);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && (!strcmp(argv[1], "--self-test") || !strcmp(argv[1], "--selftest")))
        return self_test();
    if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        printf("usage: %s [--self-test] [--bench]\n", argv[0]);
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
        bench_once(1 << 16, 200);
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

