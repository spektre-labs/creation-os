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
#include <limits.h>
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

static int self_test_buffer_helpers(void)
{
    size_t r, f;
    cos_lw_buffer_sizes(0, &r, &f);
    if (r != 0u || f != 0u) {
        fprintf(stderr, "FAIL cos_lw_buffer_sizes(0)\n");
        return 2;
    }
    cos_lw_buffer_sizes(1, &r, &f);
    if (r != 64u || f != 64u) {
        fprintf(stderr, "FAIL cos_lw_buffer_sizes(1) rep=%zu flt=%zu\n", r, f);
        return 2;
    }
    cos_lw_buffer_sizes(65, &r, &f);
    if (r != 128u || f != 320u) {
        fprintf(stderr, "FAIL cos_lw_buffer_sizes(65) rep=%zu flt=%zu\n", r, f);
        return 2;
    }
    if (cos_aligned_size_up(100u, 64u) != 128u) {
        fprintf(stderr, "FAIL cos_aligned_size_up\n");
        return 2;
    }
    if (cos_aligned_size_up(SIZE_MAX, 2u) != 0u) {
        fprintf(stderr, "FAIL cos_aligned_size_up(SIZE_MAX,2) overflow\n");
        return 2;
    }
    {
        cos_lw_owned_buffers_t ob;
        if (cos_lw_buffers_alloc(-1, &ob) == 0) {
            fprintf(stderr, "FAIL cos_lw_buffers_alloc(-1) should fail\n");
            cos_lw_buffers_free(&ob);
            return 2;
        }
        if (cos_lw_buffers_alloc(8, &ob) != 0) {
            fprintf(stderr, "FAIL cos_lw_buffers_alloc(8)\n");
            return 2;
        }
        if (!ob.reputation || !ob.logits || ob.reputation_bytes < 64u || ob.logits_bytes < 64u) {
            fprintf(stderr, "FAIL cos_lw_buffers_alloc(8) fields\n");
            cos_lw_buffers_free(&ob);
            return 2;
        }
        cos_lw_buffers_free(&ob);
    }
    return 0;
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
    static const int sizes[] = {1, 2, 7, 15, 16, 17, 33, 48, 63, 64, 65, 127, 128, 129, 1000, 4096, 65535};
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        int n = sizes[s];
        size_t rep_sz, flt_sz;
        cos_lw_buffer_sizes(n, &rep_sz, &flt_sz);
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
    size_t rep_sz, flt_sz;
    cos_lw_buffer_sizes(n, &rep_sz, &flt_sz);
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

/* Vocab above parallel threshold with a non-64 tail (parallel chunks + scalar tail). */
static int self_test_neon_parallel_with_tail(void)
{
    const int vocab = 65536 + 29;
    const float scale = 0.125f;
    size_t rep_sz, flt_sz;
    cos_lw_buffer_sizes(vocab, &rep_sz, &flt_sz);
    uint8_t *rep = (uint8_t *)aligned_alloc(64, rep_sz);
    float *base = (float *)aligned_alloc(64, flt_sz);
    float *neon = (float *)aligned_alloc(64, flt_sz);
    float *par = (float *)aligned_alloc(64, flt_sz);
    if (!rep || !base || !neon || !par) {
        fprintf(stderr, "FAIL alloc (parallel+tail n=%d)\n", vocab);
        free(rep);
        free(base);
        free(neon);
        free(par);
        return 2;
    }
    for (int i = 0; i < vocab; i++) {
        rep[i] = (uint8_t)((i * 59U + 3U) & 0xFFU);
        base[i] = 0.0f;
        neon[i] = 0.0f;
        par[i] = 0.0f;
    }
    cos_living_weights_inplace(base, rep, vocab, scale);
    cos_living_weights_neon_range(neon, rep, 0, vocab, scale);
    if (!float_buffers_exact(base, neon, vocab)) {
        fprintf(stderr, "FAIL NEON range vs scalar (parallel+tail n=%d)\n", vocab);
        free(rep);
        free(base);
        free(neon);
        free(par);
        return 2;
    }
    for (int i = 0; i < vocab; i++)
        par[i] = 0.0f;
    cos_living_weights_neon_parallel(par, rep, vocab, scale);
    if (!float_buffers_exact(base, par, vocab)) {
        fprintf(stderr, "FAIL NEON parallel vs scalar (parallel+tail n=%d)\n", vocab);
        free(rep);
        free(base);
        free(neon);
        free(par);
        return 2;
    }
    free(rep);
    free(base);
    free(neon);
    free(par);
    return 0;
}

/* Shared: scalar inplace vs NEON+GCD parallel (exact float equality). */
static int self_test_parallel_matches_inplace(int vocab, unsigned mul, unsigned add, const char *tag)
{
    const float scale = 0.125f;
    size_t rep_sz, flt_sz;
    cos_lw_buffer_sizes(vocab, &rep_sz, &flt_sz);
    uint8_t *rep = (uint8_t *)aligned_alloc(64, rep_sz);
    float *base = (float *)aligned_alloc(64, flt_sz);
    float *par = (float *)aligned_alloc(64, flt_sz);
    if (!rep || !base || !par) {
        fprintf(stderr, "FAIL alloc %s (n=%d)\n", tag, vocab);
        free(rep);
        free(base);
        free(par);
        return 2;
    }
    for (int i = 0; i < vocab; i++) {
        rep[i] = (uint8_t)(((unsigned)i * mul + add) & 0xFFU);
        base[i] = 0.0f;
        par[i] = 0.0f;
    }
    cos_living_weights_inplace(base, rep, vocab, scale);
    cos_living_weights_neon_parallel(par, rep, vocab, scale);
    if (!float_buffers_exact(base, par, vocab)) {
        fprintf(stderr, "FAIL %s NEON parallel vs scalar (n=%d)\n", tag, vocab);
        free(rep);
        free(base);
        free(par);
        return 2;
    }
    free(rep);
    free(base);
    free(par);
    return 0;
}

/* Two GCD chunks (65536*2) plus non-64 tail — exercises dispatch_apply with chunks>1. */
static int self_test_neon_parallel_two_chunks(void)
{
    return self_test_parallel_matches_inplace(65536 * 2 + 41, 41U, 11U, "two-chunk");
}

/* Three GCD chunks (65536*3) plus tail — stresses multi-chunk dispatch_apply. */
static int self_test_neon_parallel_three_chunks(void)
{
    return self_test_parallel_matches_inplace(65536 * 3 + 19, 73U, 5U, "three-chunk");
}

/* Four GCD chunks — further stress on chunk scheduling + tail. */
static int self_test_neon_parallel_four_chunks(void)
{
    return self_test_parallel_matches_inplace(65536 * 4 + 31, 97U, 13U, "four-chunk");
}

static int self_test(void)
{
    int e = self_test_buffer_helpers();
    if (e != 0)
        return e;
    e = self_test_edge_sizes();
    if (e != 0)
        return e;
    e = self_test_neon_range_slice();
    if (e != 0)
        return e;
    e = self_test_neon_parallel_with_tail();
    if (e != 0)
        return e;
    e = self_test_neon_parallel_two_chunks();
    if (e != 0)
        return e;
    e = self_test_neon_parallel_three_chunks();
    if (e != 0)
        return e;
    e = self_test_neon_parallel_four_chunks();
    if (e != 0)
        return e;

    const int vocab = 65536; /* triggers NEON parallel path on Apple AArch64 */
    const float scale = 0.125f;
    int self_big_rc = 0;
    size_t rep_sz, flt_sz;
    cos_lw_buffer_sizes(vocab, &rep_sz, &flt_sz);
    uint8_t *rep = (uint8_t *)aligned_alloc(64, rep_sz);
    float *base = (float *)aligned_alloc(64, flt_sz);
    float *neon = (float *)aligned_alloc(64, flt_sz);
    float *par = (float *)aligned_alloc(64, flt_sz);
    float *metal = (float *)aligned_alloc(64, flt_sz);
    if (!rep || !base || !neon || !par || !metal) {
        fprintf(stderr, "FAIL alloc\n");
        self_big_rc = 2;
        goto self_big_cleanup;
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
        self_big_rc = 2;
        goto self_big_cleanup;
    }

    for (int i = 0; i < vocab; i++)
        par[i] = 0.0f;
    cos_living_weights_neon_parallel(par, rep, vocab, scale);
    if (!float_buffers_exact(base, par, vocab)) {
        fprintf(stderr, "FAIL NEON parallel mismatch vs scalar\n");
        self_big_rc = 2;
        goto self_big_cleanup;
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
    {
        bool ok_metal = cos_living_weights_metal(metal, rep, vocab, scale);
        if (ok_metal) {
            if (!float_buffers_close(base, metal, vocab, 2e-3f)) {
                fprintf(stderr, "FAIL Metal mismatch vs scalar\n");
                self_big_rc = 2;
                goto self_big_cleanup;
            }
            fprintf(stderr, "native-m4: Metal path: OK\n");
        } else {
            fprintf(stderr, "native-m4: Metal path: SKIP (no metallib or Metal unavailable)\n");
        }
    }

    fprintf(stderr, "native-m4: SME runtime probe: %s\n", cos_runtime_has_sme() ? "yes" : "no");
    fprintf(stderr, "native-m4: self-test OK\n");

self_big_cleanup:
    free(rep);
    free(base);
    free(neon);
    free(par);
    free(metal);
    return self_big_rc;
}

typedef enum {
    BENCH_MODE_NEON_RANGE = 0,
    BENCH_MODE_NEON_PARALLEL = 1,
    BENCH_MODE_SCALAR = 2,
    BENCH_MODE_METAL = 3,
} bench_mode_t;

/* One timed iteration body. Returns 0, or 2 if Metal path unavailable. */
static int bench_run_body(bench_mode_t mode, float *logits, uint8_t *rep, int vocab, size_t logits_bytes)
{
    memset(logits, 0, logits_bytes);
    switch (mode) {
    case BENCH_MODE_SCALAR:
        cos_living_weights_inplace(logits, rep, vocab, 0.125f);
        return 0;
    case BENCH_MODE_NEON_PARALLEL:
        cos_living_weights_neon_parallel(logits, rep, vocab, 0.125f);
        return 0;
    case BENCH_MODE_METAL:
#if defined(__APPLE__)
        if (!cos_living_weights_metal(logits, rep, vocab, 0.125f))
            return 2;
        return 0;
#else
        return 2;
#endif
    default:
        cos_living_weights_neon_range(logits, rep, 0, vocab, 0.125f);
        return 0;
    }
}

static void bench_once(int vocab, int iters, int warmup, bench_mode_t mode)
{
    cos_lw_owned_buffers_t b;
    if (cos_lw_buffers_alloc(vocab, &b) != 0) {
        fprintf(stderr, "bench: alloc failed\n");
        return;
    }
    uint8_t *rep = b.reputation;
    float *logits = b.logits;
    for (int i = 0; i < vocab; i++)
        rep[i] = (uint8_t)(i & 0xFF);

    for (int w = 0; w < warmup; w++) {
        int r = bench_run_body(mode, logits, rep, vocab, b.logits_bytes);
        if (r != 0) {
            fprintf(stderr, "bench: Metal: SKIP (no device/metallib)\n");
            cos_lw_buffers_free(&b);
            return;
        }
    }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int k = 0; k < iters; k++) {
        int r = bench_run_body(mode, logits, rep, vocab, b.logits_bytes);
        if (r != 0) {
            fprintf(stderr, "bench: Metal: SKIP (no device/metallib)\n");
            cos_lw_buffers_free(&b);
            return;
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
    double tok_per_s = (double)iters * (double)vocab / sec;
    const char *tag = (mode == BENCH_MODE_SCALAR) ? "scalar"
                     : (mode == BENCH_MODE_NEON_PARALLEL) ? "NEON+GCD"
                     : (mode == BENCH_MODE_METAL) ? "Metal"
                                                  : "NEON";
    if (warmup > 0)
        fprintf(stderr, "bench: %s vocab=%d warmup=%d iters=%d -> %.3e tokens/s (wall)\n", tag, vocab, warmup,
                iters, tok_per_s);
    else
        fprintf(stderr, "bench: %s vocab=%d iters=%d -> %.3e tokens/s (wall)\n", tag, vocab, iters, tok_per_s);

    cos_lw_buffers_free(&b);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "--version")) {
        printf("creation_os_native_m4 (native_m4/) — Creation OS lab binary; opt-in, not merge-gate.\n");
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "--layers-report")) {
        cos_runtime_layers_report_print(stdout);
        return 0;
    }
    if (argc >= 2 && (!strcmp(argv[1], "--self-test") || !strcmp(argv[1], "--selftest")))
        return self_test();
    if (argc >= 2 && !strcmp(argv[1], "--buffer-sizes")) {
        int v = 65536;
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--vocab") && i + 1 < argc)
                v = atoi(argv[++i]);
        }
        size_t rb, lb;
        cos_lw_buffer_sizes(v, &rb, &lb);
        printf("vocab=%d reputation_bytes=%zu logits_bytes=%zu\n", v, rb, lb);
        return 0;
    }
    if (argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        printf("usage: %s [--version] [--layers-report] [--self-test] [--buffer-sizes [--vocab N]]\n"
               "         [--bench [--vocab N] [--iters K] [--warmup W] [--parallel] [--scalar] [--metal]]\n",
               argv[0]);
        printf("  (default with no args: small NEON demo on interactive GCD queue)\n");
        printf("  --bench requires positive --vocab and --iters (defaults: 65536, 200).\n");
        printf("  --layers-report prints kernel/hardware facts (uname, SME probe, buffer sizes, metallib paths).\n");
        printf("  --warmup untimed iterations before the timed block (default 0).\n");
        printf("  --scalar also runs scalar inplace for A/B vs NEON.\n");
        printf("  --metal runs Metal living-weights after NEON (SKIP if no metallib).\n");
        printf("\n");
        printf("Native M4 lab binary (opt-in). Not part of merge-gate.\n");
        printf("\n");
        printf("Optional:\n");
        printf("  CREATION_OS_METALLIB=path/to/creation_os_lw.metallib\n");
        printf("  CREATION_OS_METAL_DEBUG=1  (stderr if metallib load fails)\n");
        printf("\n");
        printf("Build metallib (Darwin):\n");
        printf("  make metallib-m4\n");
        printf("Quick bench (Makefile): make bench-native-m4\n");
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "--bench")) {
        int vocab = 1 << 16;
        int iters = 200;
        int parallel = 0;
        int scalar_ab = 0;
        int metal_bench = 0;
        int warmup = 0;
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--vocab") && i + 1 < argc)
                vocab = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--iters") && i + 1 < argc)
                iters = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--warmup") && i + 1 < argc)
                warmup = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--parallel"))
                parallel = 1;
            else if (!strcmp(argv[i], "--scalar"))
                scalar_ab = 1;
            else if (!strcmp(argv[i], "--metal"))
                metal_bench = 1;
        }
        if (vocab <= 0 || iters <= 0 || warmup < 0) {
            fprintf(stderr, "bench: --vocab and --iters must be positive; --warmup must be >= 0\n");
            return 2;
        }
        if (scalar_ab) {
            fprintf(stderr, "bench: scalar inplace (baseline)...\n");
            bench_once(vocab, iters, warmup, BENCH_MODE_SCALAR);
        }
        bench_once(vocab, iters, warmup, parallel ? BENCH_MODE_NEON_PARALLEL : BENCH_MODE_NEON_RANGE);
        if (parallel == 0 && vocab >= 65536) {
            fprintf(stderr, "bench: also running NEON+GCD (--parallel) for large vocab...\n");
            bench_once(vocab, iters, warmup, BENCH_MODE_NEON_PARALLEL);
        }
#if defined(__APPLE__)
        if (metal_bench) {
            fprintf(stderr, "bench: Metal (one kernel launch per iter; best-effort)...\n");
            bench_once(vocab, iters, warmup, BENCH_MODE_METAL);
        }
#else
        (void)metal_bench;
#endif
        return 0;
    }

    /* Demo: run living-weights on a perf queue. */
    const int vocab = 1024;
    cos_lw_owned_buffers_t b;
    if (cos_lw_buffers_alloc(vocab, &b) != 0) {
        fprintf(stderr, "alloc failed\n");
        return 2;
    }
    for (int i = 0; i < vocab; i++)
        b.reputation[i] = (uint8_t)(i & 0xFF);

    dispatch_queue_t q = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
    dispatch_sync(q, ^{
      cos_living_weights_neon(b.logits, b.reputation, vocab, 0.125f);
    });

    printf("native-m4: logits[0]=%.3f logits[1]=%.3f (demo)\n", b.logits[0], b.logits[1]);
    cos_lw_buffers_free(&b);
    return 0;
}

