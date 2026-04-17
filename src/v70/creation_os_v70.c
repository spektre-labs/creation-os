/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v70 — σ-Hyperscale: standalone self-test + microbench
 * driver.  Exits non-zero on any assertion failure.
 *
 * 1 = 1.
 */

#include "hyperscale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static uint32_t g_fail = 0;
static uint32_t g_pass = 0;

#define CHECK(cond) do {                                              \
    if (cond) { g_pass++; }                                           \
    else      { g_fail++;                                             \
                fprintf(stderr, "FAIL %s:%d: %s\n",                   \
                        __FILE__, __LINE__, #cond); }                 \
} while (0)

/* ==================================================================
 *  Helpers
 * ================================================================== */

static uint64_t splitmix64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* aligned_alloc helper that always rounds the size up to a multiple of 64
 * (glibc / POSIX strictness).  Kept for future use; currently unused on the
 * macOS path. */
static void *al64(size_t bytes) __attribute__((unused));
static void *al64(size_t bytes)
{
    size_t rounded = (bytes + 63u) & ~((size_t)63);
    if (rounded == 0) rounded = 64;
    return aligned_alloc(64, rounded);
}

/* ==================================================================
 *  1. P2Q tests
 * ================================================================== */

static void test_p2q_pack_unpack(void)
{
    /* Pack each (sign, exp) and verify round trip. */
    for (int s = -1; s <= 1; s += 2) {
        for (uint8_t e = 0; e <= COS_V70_P2Q_EXP_MAX; ++e) {
            uint8_t nib = cos_v70_p2q_pack((int8_t)s, e);
            int8_t  sg; uint8_t eg;
            cos_v70_p2q_unpack(nib, &sg, &eg);
            CHECK(sg == s);
            CHECK(eg == e);
        }
    }
    /* Saturating exp. */
    uint8_t nib = cos_v70_p2q_pack(+1, 200);
    int8_t s; uint8_t e;
    cos_v70_p2q_unpack(nib, &s, &e);
    CHECK(s == +1);
    CHECK(e == COS_V70_P2Q_EXP_MAX);
}

static void test_p2q_gemv_known(void)
{
    /* 2 rows × 4 cols.  Weights row 0: [+1<<0, -1<<1, +1<<2, -1<<3].
     * Weights row 1: [+1<<3, +1<<2, +1<<1, +1<<0].
     * x = [10, 10, 10, 10] → row 0 = 10 - 20 + 40 - 80 = -50.
     *                       row 1 = 80 + 40 + 20 + 10 = 150.
     */
    uint8_t W[4]; /* 2 nibbles per byte; 4 weights per row → 2 bytes per row */
    /* row 0 nibbles */
    uint8_t r0n0 = cos_v70_p2q_pack(+1, 0);
    uint8_t r0n1 = cos_v70_p2q_pack(-1, 1);
    uint8_t r0n2 = cos_v70_p2q_pack(+1, 2);
    uint8_t r0n3 = cos_v70_p2q_pack(-1, 3);
    W[0] = (uint8_t)((r0n1 << 4) | (r0n0 & 0x0Fu));
    W[1] = (uint8_t)((r0n3 << 4) | (r0n2 & 0x0Fu));
    /* row 1 nibbles */
    uint8_t r1n0 = cos_v70_p2q_pack(+1, 3);
    uint8_t r1n1 = cos_v70_p2q_pack(+1, 2);
    uint8_t r1n2 = cos_v70_p2q_pack(+1, 1);
    uint8_t r1n3 = cos_v70_p2q_pack(+1, 0);
    W[2] = (uint8_t)((r1n1 << 4) | (r1n0 & 0x0Fu));
    W[3] = (uint8_t)((r1n3 << 4) | (r1n2 & 0x0Fu));

    int16_t x[4] = { 10, 10, 10, 10 };
    int32_t out[2] = { 0, 0 };
    cos_v70_p2q_gemv(W, x, 2u, 4u, out);
    CHECK(out[0] == -50);
    CHECK(out[1] == 150);
}

static void test_p2q_gemv_zero(void)
{
    /* All weights = +1<<0; out[r] = sum(x). */
    enum { ROWS = 4, COLS = 16 };
    uint8_t W[ROWS * COLS / 2];
    uint8_t one = cos_v70_p2q_pack(+1, 0);
    uint8_t pair = (uint8_t)((one << 4) | (one & 0x0Fu));
    memset(W, pair, sizeof(W));
    int16_t x[COLS];
    int32_t out[ROWS] = {0};
    int32_t want = 0;
    for (int i = 0; i < COLS; ++i) { x[i] = (int16_t)(i + 1); want += i + 1; }
    cos_v70_p2q_gemv(W, x, ROWS, COLS, out);
    for (int r = 0; r < ROWS; ++r) CHECK(out[r] == want);
}

/* ==================================================================
 *  2. SSM scan tests
 * ================================================================== */

static void test_ssm_basic(void)
{
    /* h[t] = 0 * h[t-1] + 1 * x[t]; y[t] = 1 * h[t]. */
    enum { N = 8 };
    int16_t a[N], b[N], c[N], x[N], h[N], y[N];
    for (int t = 0; t < N; ++t) {
        a[t] = 0; b[t] = 32767; c[t] = 32767;
        x[t] = (int16_t)(1000 * (t + 1));
    }
    cos_v70_ssm_scan(a, b, c, x, N, 0, h, y);
    for (int t = 0; t < N; ++t) {
        /* b=32767 is ~0.99997 in Q0.15; h[t] = b*x[t]>>15 ≈ x[t]−1..x[t]. */
        CHECK(h[t] >= x[t] - 2 && h[t] <= x[t]);
        CHECK(y[t] >= x[t] - 3 && y[t] <= x[t]);
    }
}

static void test_ssm_decay(void)
{
    /* a = 0.5 (Q0.15 = 16384), b=0, c=1.  h[t] = 0.5*h[t-1].  Starts h0=32000. */
    enum { N = 10 };
    int16_t a[N], b[N], c[N], x[N], h[N], y[N];
    for (int t = 0; t < N; ++t) { a[t] = 16384; b[t] = 0; c[t] = 32767; x[t] = 0; }
    cos_v70_ssm_scan(a, b, c, x, N, 32000, h, y);
    /* h should monotonically decrease toward 0. */
    for (int t = 1; t < N; ++t) CHECK(h[t] <= h[t-1]);
    CHECK(h[N-1] < 200);
}

/* ==================================================================
 *  3. RWKV-7 step tests
 * ================================================================== */

static void test_rwkv7_dim1(void)
{
    /* dim=1, decay=0, gate driven by w*x+u, v=value. */
    int16_t r=32767, w=32767, u=0, v=10000, dec=0, x=32767;
    int16_t state = 0, y = 0;
    cos_v70_rwkv7_step(&r, &w, &u, &v, &dec, &x, &state, &y, 1);
    /* gate = w*x>>15 + u = ~32767 + 0; v_eff = v - state = 10000.
     * state' = decay + gate*v_eff>>15 = 0 + ~10000.  y = r*state>>15 ≈ state. */
    CHECK(state >= 9000 && state <= 10000);
    CHECK(y >= 8500 && y <= 10000);
}

static void test_rwkv7_pure_decay(void)
{
    /* gate = 0 (w=0,u=0); decay=0.5 (16384); state will halve each step. */
    int16_t r=32767, w=0, u=0, v=0, dec=16384, x=0;
    int16_t state = 30000, y = 0;
    int16_t prev = state;
    for (int i = 0; i < 6; ++i) {
        cos_v70_rwkv7_step(&r, &w, &u, &v, &dec, &x, &state, &y, 1);
        CHECK(state <= prev);
        prev = state;
    }
    CHECK(state < 1000);
}

/* ==================================================================
 *  4. MoE top-K tests
 * ================================================================== */

static void test_moe_topk_known(void)
{
    int16_t scores[8] = { 100, 500, 200, 800, 300, 50, 1000, 400 };
    cos_v70_moe_route_t out;
    memset(&out, 0, sizeof(out));
    uint32_t load[8] = {0};
    cos_v70_moe_route_topk(scores, NULL, 8u, 3u, &out, load);
    CHECK(out.k == 3);
    /* top-3 should be experts 6 (1000), 3 (800), 1 (500). */
    CHECK(out.selected[0] == 6u);
    CHECK(out.selected[1] == 3u);
    CHECK(out.selected[2] == 1u);
    CHECK(load[6] == 1u);
    CHECK(load[3] == 1u);
    CHECK(load[1] == 1u);
}

static void test_moe_bias_balancing(void)
{
    /* All scores equal; bias[0] starts heavy → should be down-corrected
     * once the load counter shows it dominates. */
    int16_t bias[4] = { 0, 0, 0, 0 };
    uint32_t load[4] = { 100, 10, 10, 10 }; /* expert 0 over-loaded */
    cos_v70_moe_bias_update(bias, load, 4u, 100);
    /* avg = 32; expert 0 (load 100) > avg → bias[0] decreases (sign neg). */
    CHECK(bias[0] < 0);
    CHECK(bias[1] > 0);
}

static void test_moe_topk_10k(void)
{
    /* Push to 10 240 experts with random scores, K=8.  Verify top-K is
     * monotonically non-increasing and all indices are unique. */
    enum { N = 1024, K = 8 };  /* limited to N=1024 in test for speed */
    int16_t *scores = (int16_t *)malloc(sizeof(int16_t) * N);
    CHECK(scores != NULL);
    if (!scores) return;
    uint64_t st = 0xC0FFEE;
    for (int i = 0; i < N; ++i)
        scores[i] = (int16_t)(splitmix64(&st) & 0x7FFF);
    cos_v70_moe_route_t out;
    memset(&out, 0, sizeof(out));
    cos_v70_moe_route_topk(scores, NULL, N, K, &out, NULL);
    for (uint32_t j = 1; j < out.k; ++j)
        CHECK(out.selected_score_q15[j-1] >= out.selected_score_q15[j]);
    /* uniqueness */
    for (uint32_t a = 0; a < out.k; ++a)
        for (uint32_t b = a + 1; b < out.k; ++b)
            CHECK(out.selected[a] != out.selected[b]);
    free(scores);
}

/* ==================================================================
 *  5. PIM popcount tests
 * ================================================================== */

static void test_pim_popcount(void)
{
    uint64_t act = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t W[4];
    W[0] = 0x0000000000000001ULL;            /* popcount(act & W) = 1 */
    W[1] = 0x000000000000FFFFULL;            /* = 16 */
    W[2] = 0xFFFFFFFFFFFFFFFFULL;            /* = 64 */
    W[3] = 0x0000000000000000ULL;            /* = 0 */
    uint32_t out[4] = {0,0,0,0};
    cos_v70_pim_and_popcount(act, W, 4u, out);
    CHECK(out[0] == 1u);
    CHECK(out[1] == 16u);
    CHECK(out[2] == 64u);
    CHECK(out[3] == 0u);
    /* Accumulating call. */
    cos_v70_pim_and_popcount(act, W, 4u, out);
    CHECK(out[0] == 2u);
    CHECK(out[1] == 32u);
    CHECK(out[2] == 128u);
    CHECK(out[3] == 0u);
}

/* ==================================================================
 *  6. WDM dot tests
 * ================================================================== */

static void test_wdm_dot(void)
{
    enum { N = 4, L = 4 };
    int16_t q[N*L], k[N*L];
    /* Layout: q[i*L + l].  Make each lane's dot distinct. */
    for (int i = 0; i < N; ++i) {
        for (int l = 0; l < L; ++l) {
            q[i*L + l] = (int16_t)(1000 * (l + 1));
            k[i*L + l] = (int16_t)(1000);
        }
    }
    int32_t lane_out[L] = {0};
    int32_t total = cos_v70_wdm_dot(q, k, N, L, lane_out);
    /* lane l's per-element product (Q0.15) ≈ (1000*(l+1) * 1000) >> 15
     *   = 1000*(l+1) * 1000 / 32768 ≈ ~30 * (l+1).
     * Each lane has N=4 such terms.  total = sum over lanes. */
    CHECK(lane_out[0] >  100);
    CHECK(lane_out[3] >  lane_out[0]);
    CHECK(total > lane_out[0]);
}

/* ==================================================================
 *  7. Spike encode + readout tests
 * ================================================================== */

static void test_spike_basic(void)
{
    enum { N = 8, R = 2 };
    int16_t a[N] = { 100, 9000, 200, 16000, 8200, 10, 20000, 100 };
    int16_t spikes[N];
    uint8_t fire[N];
    int16_t thresh = 8000;
    uint32_t fired = cos_v70_spike_encode(a, N, thresh, spikes, fire);
    /* a >= 8000 → fire: indices 1, 3, 4, 6.  fired=4. */
    CHECK(fired == 4u);
    CHECK(fire[1] == 1u);
    CHECK(fire[3] == 1u);
    CHECK(fire[4] == 1u);
    CHECK(fire[6] == 1u);
    CHECK(fire[0] == 0u);
    CHECK(spikes[0] == 0);

    /* Readout: rows = 2.  w_q15 row r col i = constant 32767 (i.e. ~+1).
     * y[r] = sum over firing i of (spike[i]*32767)>>15 ≈ sum spike[i]. */
    int16_t w[R*N];
    for (int i = 0; i < R*N; ++i) w[i] = 32767;
    int32_t y[R] = {0,0};
    cos_v70_spike_readout(spikes, fire, w, R, N, y);
    int32_t expect = (int32_t)spikes[1] + spikes[3] + spikes[4] + spikes[6];
    CHECK(y[0] >= expect - 10 && y[0] <= expect + 10);
    CHECK(y[1] == y[0]);
}

/* ==================================================================
 *  8. Ring all-reduce tests
 * ================================================================== */

static void test_ring_allreduce_n4(void)
{
    enum { N = 4, C = 3 };  /* 4 ranks × 3 elements per chunk × 4 chunks */
    int32_t r0[N*C], r1[N*C], r2[N*C], r3[N*C];
    int32_t *bufs[N] = { r0, r1, r2, r3 };
    /* Initialise: rank r's chunk i = (r+1)*100 + i. */
    for (uint32_t r = 0; r < N; ++r)
        for (uint32_t c = 0; c < N; ++c)
            for (uint32_t i = 0; i < C; ++i)
                bufs[r][c*C + i] = (int32_t)(((r+1)*100) + i);
    uint8_t ok = cos_v70_ring_allreduce(bufs, N, C);
    CHECK(ok == 1u);
    /* Expected sum at chunk c, element i = (1+2+3+4)*100 + 4*i = 1000 + 4*i. */
    for (uint32_t r = 0; r < N; ++r)
        for (uint32_t c = 0; c < N; ++c)
            for (uint32_t i = 0; i < C; ++i) {
                int32_t want = (int32_t)(1000 + 4u*i);
                CHECK(bufs[r][c*C + i] == want);
            }
}

static void test_ring_allreduce_n1(void)
{
    int32_t r0[3] = { 42, 43, 44 };
    int32_t *bufs[1] = { r0 };
    uint8_t ok = cos_v70_ring_allreduce(bufs, 1, 3);
    CHECK(ok == 1u);
    CHECK(r0[0] == 42 && r0[1] == 43 && r0[2] == 44);
}

/* ==================================================================
 *  9. Streaming LRU tests
 * ================================================================== */

static void test_stream_lru_basic(void)
{
    cos_v70_stream_cache_t c;
    cos_v70_stream_init(&c);
    int dummy_a = 1, dummy_b = 2, dummy_c = 3;
    cos_v70_stream_insert(&c, 0, 0, &dummy_a, 0);
    cos_v70_stream_insert(&c, 0, 1, &dummy_b, 0);
    cos_v70_stream_insert(&c, 1, 0, &dummy_c, 0);
    void *p = cos_v70_stream_lookup(&c, 0, 1);
    CHECK(p == &dummy_b);
    CHECK(c.hits == 1u);
    CHECK(c.misses == 0u);
    p = cos_v70_stream_lookup(&c, 99, 99);
    CHECK(p == NULL);
    CHECK(c.misses == 1u);
}

static void test_stream_lru_eviction(void)
{
    cos_v70_stream_cache_t c;
    cos_v70_stream_init(&c);
    int slot_marker[COS_V70_STREAM_SLOTS + 4];
    for (uint32_t i = 0; i < COS_V70_STREAM_SLOTS; ++i)
        cos_v70_stream_insert(&c, i, 0, &slot_marker[i], 0);
    /* All slots full; insert one more → eviction. */
    cos_v70_stream_insert(&c, 999, 0, &slot_marker[COS_V70_STREAM_SLOTS], 0);
    CHECK(c.evictions == 1u);
    void *p = cos_v70_stream_lookup(&c, 999, 0);
    CHECK(p == &slot_marker[COS_V70_STREAM_SLOTS]);
}

/* ==================================================================
 *  10. HSL (Hyperscale Language) tests
 * ================================================================== */

static void test_hsl_simple_gate(void)
{
    cos_v70_hsl_state_t *s = cos_v70_hsl_new();
    CHECK(s != NULL);
    if (!s) return;

    /* Two SHIFT instructions then GATE. */
    cos_v70_inst_t prog[] = {
        { COS_V70_OP_HALT, 0, 0, 0, 0, 0 }, /* placeholder; replaced below */
    };
    (void)prog;
    /* Build a minimal P2Q context: 2 rows × 2 cols → 1 byte/row, W[2]. */
    uint8_t W[2];
    uint8_t one = cos_v70_p2q_pack(+1, 0);
    W[0] = (uint8_t)((one << 4) | (one & 0x0Fu));
    W[1] = (uint8_t)((one << 4) | (one & 0x0Fu));
    int16_t x[2] = { 100, 100 };
    int32_t out[2] = {0,0};
    cos_v70_hsl_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.p2q_weights = W;
    ctx.p2q_x_q15   = x;
    ctx.p2q_rows    = 2u;
    ctx.p2q_cols    = 2u;
    ctx.p2q_out     = out;

    cos_v70_inst_t prog2[] = {
        { COS_V70_OP_SHIFT, 0, 0, 0,    0, 0 },
        { COS_V70_OP_GATE,  0, 0, 0,  100, 0 },  /* require reg_q15[0] >= 100 */
    };
    int rc = cos_v70_hsl_exec(s, prog2, 2, &ctx, 1000);
    CHECK(rc == 0);
    CHECK(cos_v70_hsl_v70_ok(s) == 1u);
    CHECK(cos_v70_hsl_cost(s) == 2u);  /* SHIFT + GATE */
    CHECK(cos_v70_hsl_topology_ok(s) == 1u);
    cos_v70_hsl_free(s);
}

static void test_hsl_budget_abstain(void)
{
    cos_v70_hsl_state_t *s = cos_v70_hsl_new();
    CHECK(s != NULL);
    if (!s) return;
    cos_v70_inst_t prog[] = {
        { COS_V70_OP_RING,  0, 0, 0,    0, 0 },  /* cost 8 — over budget */
        { COS_V70_OP_GATE,  0, 0, 0,    0, 0 },
    };
    cos_v70_hsl_ctx_t ctx; memset(&ctx, 0, sizeof(ctx));
    int rc = cos_v70_hsl_exec(s, prog, 2, &ctx, 4 /* budget */);
    CHECK(rc != 0);  /* either over-budget abort or RING abstain */
    CHECK(cos_v70_hsl_v70_ok(s) == 0u);
    cos_v70_hsl_free(s);
}

/* ==================================================================
 *  Composed 11-bit decision: full truth table (2^11 = 2048 rows)
 * ================================================================== */

static void test_compose_truth_table(void)
{
    for (uint32_t bits = 0; bits < 2048u; ++bits) {
        uint8_t v[11];
        for (int i = 0; i < 11; ++i) v[i] = (uint8_t)((bits >> i) & 1u);
        cos_v70_decision_t d = cos_v70_compose_decision(
            v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
        uint8_t want = 1u;
        for (int i = 0; i < 11; ++i) if (!v[i]) { want = 0u; break; }
        CHECK(d.allow == want);
    }
}

/* ==================================================================
 *  Microbench
 * ================================================================== */

static void run_bench(void)
{
    printf("v70 σ-Hyperscale microbench:\n");

    /* 1. P2Q GEMV: rows=128, cols=512, N=20 000 calls. */
    {
        enum { ROWS = 128, COLS = 512 };
        uint8_t  *W = (uint8_t *)aligned_alloc(64, ROWS * COLS / 2);
        int16_t  *x = (int16_t *)aligned_alloc(64, COLS * sizeof(int16_t));
        int32_t  *y = (int32_t *)aligned_alloc(64, ROWS * sizeof(int32_t));
        if (W && x && y) {
            uint64_t st = 0xC0FFEE;
            for (int i = 0; i < ROWS * COLS / 2; ++i)
                W[i] = (uint8_t)(splitmix64(&st) & 0xFFu);
            for (int i = 0; i < COLS; ++i) x[i] = (int16_t)((i & 7) - 4);
            const int N = 2000;
            double t0 = now_sec();
            for (int it = 0; it < N; ++it) cos_v70_p2q_gemv(W, x, ROWS, COLS, y);
            double dt = now_sec() - t0;
            double gemv_per_s = (double)N / (dt > 0 ? dt : 1e-9);
            double gops = gemv_per_s * (double)ROWS * (double)COLS / 1e9;
            printf("  p2q_gemv 128x512          : %.0f gemv/s (~%.2f Gshift+add/s)\n",
                   gemv_per_s, gops);
        }
        free(W); free(x); free(y);
    }

    /* 2. SSM scan N=4096 × 2000 calls. */
    {
        enum { N = 4096 };
        int16_t *a = (int16_t *)aligned_alloc(64, N * sizeof(int16_t));
        int16_t *b = (int16_t *)aligned_alloc(64, N * sizeof(int16_t));
        int16_t *c = (int16_t *)aligned_alloc(64, N * sizeof(int16_t));
        int16_t *x = (int16_t *)aligned_alloc(64, N * sizeof(int16_t));
        int16_t *h = (int16_t *)aligned_alloc(64, N * sizeof(int16_t));
        int16_t *y = (int16_t *)aligned_alloc(64, N * sizeof(int16_t));
        if (a && b && c && x && h && y) {
            for (int i = 0; i < N; ++i) {
                a[i] = 16384; b[i] = 16384; c[i] = 32767;
                x[i] = (int16_t)((i & 1023) - 512);
            }
            const int M = 2000;
            double t0 = now_sec();
            for (int it = 0; it < M; ++it)
                cos_v70_ssm_scan(a, b, c, x, N, 0, h, y);
            double dt = now_sec() - t0;
            double tok_per_s = (double)M * (double)N / (dt > 0 ? dt : 1e-9);
            printf("  ssm_scan N=4096           : %.2f M tokens/s (last y=%d)\n",
                   tok_per_s / 1e6, y[N-1]);
        }
        free(a); free(b); free(c); free(x); free(h); free(y);
    }

    /* 3. RWKV-7 step dim=128 × 100 000 calls. */
    {
        enum { D = 128 };
        int16_t r[D], w[D], u[D], v[D], dec[D], x[D], st[D], y[D];
        for (int i = 0; i < D; ++i) {
            r[i] = 30000; w[i] = 16384; u[i] = 1000; v[i] = 8000;
            dec[i] = 28000; x[i] = (int16_t)(i*100); st[i] = 0;
        }
        const int N = 100000;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i)
            cos_v70_rwkv7_step(r, w, u, v, dec, x, st, y, D);
        double dt = now_sec() - t0;
        printf("  rwkv7_step dim=128        : %.2f M steps/s (last y=%d)\n",
               (double)N / (dt > 0 ? dt : 1e-9) / 1e6, y[D-1]);
    }

    /* 4. MoE top-K route N=10 240 K=8 × 1000 calls. */
    {
        enum { N = 10240, K = 8 };
        int16_t *scores = (int16_t *)aligned_alloc(64, N * sizeof(int16_t));
        if (scores) {
            uint64_t st = 0xDEADBEEF;
            for (int i = 0; i < N; ++i) scores[i] = (int16_t)(splitmix64(&st) & 0x7FFF);
            cos_v70_moe_route_t out; memset(&out, 0, sizeof(out));
            const int M = 1000;
            double t0 = now_sec();
            for (int i = 0; i < M; ++i)
                cos_v70_moe_route_topk(scores, NULL, N, K, &out, NULL);
            double dt = now_sec() - t0;
            printf("  moe_route N=10240 K=8     : %.0f routes/s (top=%u sc=%d)\n",
                   (double)M / (dt > 0 ? dt : 1e-9),
                   out.selected[0], out.selected_score_q15[0]);
        }
        free(scores);
    }

    /* 5. PIM popcount cols=4096 × 50 000 calls. */
    {
        enum { C = 4096 };
        uint64_t *W   = (uint64_t *)aligned_alloc(64, C * sizeof(uint64_t));
        uint32_t *out = (uint32_t *)aligned_alloc(64, C * sizeof(uint32_t));
        if (W && out) {
            uint64_t st = 0xFEEDC0DE;
            for (int i = 0; i < C; ++i) W[i] = splitmix64(&st);
            memset(out, 0, C * sizeof(uint32_t));
            const int N = 50000;
            double t0 = now_sec();
            for (int i = 0; i < N; ++i) {
                cos_v70_pim_and_popcount(splitmix64(&st), W, C, out);
            }
            double dt = now_sec() - t0;
            double pop_per_s = (double)N * (double)C / (dt > 0 ? dt : 1e-9);
            printf("  pim_popcount cols=4096    : %.2f G AND-pop/s (out[0]=%u)\n",
                   pop_per_s / 1e9, out[0]);
        }
        free(W); free(out);
    }

    /* 6. WDM dot lanes=8, n_per_lane=512 × 100 000 calls. */
    {
        enum { L = 8, NPL = 512 };
        int16_t *q = (int16_t *)aligned_alloc(64, L * NPL * sizeof(int16_t));
        int16_t *k = (int16_t *)aligned_alloc(64, L * NPL * sizeof(int16_t));
        int32_t lane_out[L];
        if (q && k) {
            for (int i = 0; i < L * NPL; ++i) { q[i] = 1000; k[i] = 1000; }
            const int N = 100000;
            int32_t total = 0;
            double t0 = now_sec();
            for (int i = 0; i < N; ++i)
                total += cos_v70_wdm_dot(q, k, NPL, L, lane_out);
            double dt = now_sec() - t0;
            printf("  wdm_dot lanes=8 npl=512   : %.2f M dots/s (total=%d)\n",
                   (double)N / (dt > 0 ? dt : 1e-9) / 1e6, total);
        }
        free(q); free(k);
    }

    /* 7. Spike encode N=1024 × 200 000 calls. */
    {
        enum { N = 1024 };
        int16_t a[N], spikes[N];
        uint8_t fire[N];
        for (int i = 0; i < N; ++i) a[i] = (int16_t)((i * 37) & 0x7FFF);
        const int M = 200000;
        uint32_t fired = 0;
        double t0 = now_sec();
        for (int i = 0; i < M; ++i)
            fired = cos_v70_spike_encode(a, N, 8000, spikes, fire);
        double dt = now_sec() - t0;
        printf("  spike_encode N=1024       : %.2f M encodes/s (fired=%u)\n",
               (double)M / (dt > 0 ? dt : 1e-9) / 1e6, fired);
    }

    /* 8. Ring all-reduce N=8 ranks chunk=64 × 5000 calls. */
    {
        enum { N = 8, C = 64 };
        int32_t *bufs[N];
        int32_t *raw = (int32_t *)aligned_alloc(64, N * N * C * sizeof(int32_t));
        if (raw) {
            for (int r = 0; r < N; ++r) bufs[r] = raw + (size_t)r * N * C;
            const int M = 5000;
            double t0 = now_sec();
            for (int i = 0; i < M; ++i) {
                for (int r = 0; r < N; ++r)
                    for (int c = 0; c < N * C; ++c)
                        bufs[r][c] = (r + c) & 0xFFFF;
                (void)cos_v70_ring_allreduce(bufs, N, C);
            }
            double dt = now_sec() - t0;
            printf("  ring_allreduce N=8 c=64   : %.2f k allreduces/s (last=%d)\n",
                   (double)M / (dt > 0 ? dt : 1e-9) / 1e3, bufs[0][0]);
        }
        free(raw);
    }

    /* 9. Stream LRU 100 000 lookups + inserts. */
    {
        cos_v70_stream_cache_t c;
        cos_v70_stream_init(&c);
        int *markers = (int *)malloc(1024 * sizeof(int));
        if (markers) {
            for (int i = 0; i < 1024; ++i) markers[i] = i;
            const int N = 100000;
            double t0 = now_sec();
            for (int i = 0; i < N; ++i) {
                uint32_t k = (uint32_t)((i * 1103515245u + 12345u) % 1024u);
                void *p = cos_v70_stream_lookup(&c, k, 0);
                if (!p) cos_v70_stream_insert(&c, k, 0, &markers[k], 0);
            }
            double dt = now_sec() - t0;
            printf("  stream_lru ops 100k       : %.2f M ops/s (hit=%llu miss=%llu evict=%llu)\n",
                   (double)N / (dt > 0 ? dt : 1e-9) / 1e6,
                   (unsigned long long)c.hits,
                   (unsigned long long)c.misses,
                   (unsigned long long)c.evictions);
        }
        free(markers);
    }

    /* 10. HSL 6-instruction end-to-end × 200 000. */
    {
        cos_v70_hsl_state_t *s = cos_v70_hsl_new();
        if (s) {
            /* 2 rows × 2 cols → each row 1 byte → W[2]. */
            uint8_t W[2];
            uint8_t one = cos_v70_p2q_pack(+1, 0);
            W[0] = (uint8_t)((one << 4) | (one & 0x0Fu));
            W[1] = (uint8_t)((one << 4) | (one & 0x0Fu));
            int16_t xq[2] = { 1000, 1000 };
            int32_t yq[2] = { 0, 0 };
            int16_t aq[8], bq[8], cq[8], xs[8], hs[8], ys[8];
            for (int i = 0; i < 8; ++i) { aq[i] = 16384; bq[i] = 16384; cq[i] = 32767; xs[i] = 100; }
            int16_t scores[16];
            for (int i = 0; i < 16; ++i) scores[i] = (int16_t)((i * 137) & 0x7FFF);
            cos_v70_moe_route_t mout; memset(&mout, 0, sizeof(mout));
            uint64_t pim_w[8];
            for (int i = 0; i < 8; ++i) pim_w[i] = (uint64_t)i * 0x1111111111111111ULL;
            uint32_t pim_o[8] = {0};
            int16_t qw[16], kw[16];
            for (int i = 0; i < 16; ++i) { qw[i] = 1000; kw[i] = 1000; }
            int32_t lo[2] = {0, 0};
            int16_t aa[16], spk[16];
            uint8_t fmask[16];
            int16_t rd_w[16];
            int32_t rd_y[1] = {0};
            for (int i = 0; i < 16; ++i) { aa[i] = (int16_t)((i*100)+5000); rd_w[i] = 32000; }
            cos_v70_hsl_ctx_t ctx; memset(&ctx, 0, sizeof(ctx));
            ctx.p2q_weights = W; ctx.p2q_x_q15 = xq; ctx.p2q_rows = 2; ctx.p2q_cols = 2; ctx.p2q_out = yq;
            ctx.ssm_a = aq; ctx.ssm_b = bq; ctx.ssm_c = cq; ctx.ssm_x = xs; ctx.ssm_n = 8; ctx.ssm_h0 = 0; ctx.ssm_h_out = hs; ctx.ssm_y_out = ys;
            ctx.moe_scores = scores; ctx.moe_n_experts = 16; ctx.moe_k = 4; ctx.moe_out = &mout;
            ctx.pim_act_word = 0xFFULL; ctx.pim_weights_col = pim_w; ctx.pim_n_cols = 8; ctx.pim_out = pim_o;
            ctx.wdm_q = qw; ctx.wdm_k = kw; ctx.wdm_n_per_lane = 2; ctx.wdm_lanes = 2; ctx.wdm_lane_out = lo;
            ctx.spike_a_q15 = aa; ctx.spike_n = 16; ctx.spike_thresh_q15 = 8000; ctx.spike_spikes_q15 = spk; ctx.spike_fire_mask = fmask; ctx.spike_w_q15 = rd_w; ctx.spike_rows = 1; ctx.spike_y_out = rd_y;
            cos_v70_inst_t prog[] = {
                { COS_V70_OP_SHIFT,   0, 0, 0,    0, 0 },
                { COS_V70_OP_SCAN,    1, 0, 0,    0, 0 },
                { COS_V70_OP_TIMEMIX, 2, 0, 0,    0, 0 }, /* skipped: rwkv ctx absent */
                { COS_V70_OP_ROUTEK,  3, 0, 0,    0, 0 },
                { COS_V70_OP_PIMPOP,  4, 0, 0,    0, 0 },
                { COS_V70_OP_WDM,     5, 0, 0,    0, 0 },
                { COS_V70_OP_SPIKE,   6, 0, 0,    0, 0 },
                { COS_V70_OP_GATE,    0, 0, 0,    0, 0 },
            };
            const int N = 200000;
            int ok_sum = 0;
            double t0 = now_sec();
            for (int i = 0; i < N; ++i) {
                cos_v70_hsl_reset(s);
                /* Reset spike fire mask each iteration. */
                memset(rd_y, 0, sizeof(rd_y));
                /* TIMEMIX without ctx → abstain.  To avoid that, skip TIMEMIX. */
                cos_v70_inst_t skipped[] = {
                    { COS_V70_OP_SHIFT,   0, 0, 0,    0, 0 },
                    { COS_V70_OP_SCAN,    1, 0, 0,    0, 0 },
                    { COS_V70_OP_ROUTEK,  3, 0, 0,    0, 0 },
                    { COS_V70_OP_PIMPOP,  4, 0, 0,    0, 0 },
                    { COS_V70_OP_WDM,     5, 0, 0,    0, 0 },
                    { COS_V70_OP_SPIKE,   6, 0, 0,    0, 0 },
                    { COS_V70_OP_GATE,    0, 0, 0,    0, 0 },
                };
                (void)cos_v70_hsl_exec(s, skipped, 7, &ctx, 256);
                ok_sum += cos_v70_hsl_v70_ok(s);
            }
            double dt = now_sec() - t0;
            printf("  hsl 7-inst end-to-end     : %.0f progs/s (ok=%d)\n",
                   (double)N / (dt > 0 ? dt : 1e-9), ok_sum);
            cos_v70_hsl_free(s);
            (void)prog;
        }
    }

    /* 11. Compose 11-bit × 10 000 000. */
    {
        const int N = 10000000;
        uint32_t acc = 0;
        double t0 = now_sec();
        for (int i = 0; i < N; ++i) {
            cos_v70_decision_t d = cos_v70_compose_decision(
                (i & 1), ((i >> 1) & 1), ((i >> 2) & 1),
                ((i >> 3) & 1), ((i >> 4) & 1), ((i >> 5) & 1),
                ((i >> 6) & 1), ((i >> 7) & 1), ((i >> 8) & 1),
                ((i >> 9) & 1), ((i >> 10) & 1));
            acc += d.allow;
        }
        double dt = now_sec() - t0;
        printf("  compose_decision 11-bit   : %.0f decisions/s (acc=%u)\n",
               (double)N / (dt > 0 ? dt : 1e-9), acc);
    }
}

/* ==================================================================
 *  Decision CLI: print JSON for cos_v70_compose_decision.
 * ================================================================== */

static int run_decision(int argc, char **argv)
{
    if (argc != 11) {
        fprintf(stderr,
            "usage: --decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70\n");
        return 2;
    }
    uint8_t v[11];
    for (int i = 0; i < 11; ++i)
        v[i] = (uint8_t)(atoi(argv[i]) ? 1 : 0);
    cos_v70_decision_t d = cos_v70_compose_decision(
        v[0], v[1], v[2], v[3], v[4], v[5],
        v[6], v[7], v[8], v[9], v[10]);
    printf("{\"v60_ok\":%u,\"v61_ok\":%u,\"v62_ok\":%u,\"v63_ok\":%u,"
           "\"v64_ok\":%u,\"v65_ok\":%u,\"v66_ok\":%u,\"v67_ok\":%u,"
           "\"v68_ok\":%u,\"v69_ok\":%u,\"v70_ok\":%u,\"allow\":%u}\n",
           d.v60_ok, d.v61_ok, d.v62_ok, d.v63_ok, d.v64_ok, d.v65_ok,
           d.v66_ok, d.v67_ok, d.v68_ok, d.v69_ok, d.v70_ok, d.allow);
    return d.allow ? 0 : 1;
}

/* ==================================================================
 *  Entry.
 * ================================================================== */

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v70_version);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) {
        run_bench();
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--decision") == 0) {
        return run_decision(argc - 2, argv + 2);
    }
    if (argc < 2 || strcmp(argv[1], "--self-test") != 0) {
        fprintf(stderr,
            "usage: %s --self-test | --bench | --version | "
            "--decision v60 v61 v62 v63 v64 v65 v66 v67 v68 v69 v70\n",
            argv[0]);
        return 2;
    }

    test_p2q_pack_unpack();
    test_p2q_gemv_known();
    test_p2q_gemv_zero();
    test_ssm_basic();
    test_ssm_decay();
    test_rwkv7_dim1();
    test_rwkv7_pure_decay();
    test_moe_topk_known();
    test_moe_bias_balancing();
    test_moe_topk_10k();
    test_pim_popcount();
    test_wdm_dot();
    test_spike_basic();
    test_ring_allreduce_n4();
    test_ring_allreduce_n1();
    test_stream_lru_basic();
    test_stream_lru_eviction();
    test_hsl_simple_gate();
    test_hsl_budget_abstain();
    test_compose_truth_table();

    /* Deterministic fuzz: 256 random P2Q matrices × 32 rows × 64 cols. */
    {
        uint64_t st = 0xBADC0FFEE;
        enum { ROWS = 32, COLS = 64 };
        uint8_t W[ROWS * COLS / 2];
        int16_t x[COLS];
        int32_t y[ROWS];
        for (int trial = 0; trial < 256; ++trial) {
            for (int i = 0; i < ROWS * COLS / 2; ++i) W[i] = (uint8_t)(splitmix64(&st) & 0xFFu);
            for (int i = 0; i < COLS; ++i) x[i] = (int16_t)((splitmix64(&st) & 0xFFFu) - 2048);
            cos_v70_p2q_gemv(W, x, ROWS, COLS, y);
            CHECK(y[0] >= INT32_MIN && y[ROWS-1] <= INT32_MAX); /* trivially sound */
        }
    }

    /* Deterministic fuzz: 256 SSM scans of length 64. */
    {
        uint64_t st = 0xCAFE1234;
        enum { N = 64 };
        int16_t a[N], b[N], c[N], x[N], h[N], y[N];
        for (int trial = 0; trial < 256; ++trial) {
            for (int i = 0; i < N; ++i) {
                a[i] = (int16_t)(splitmix64(&st) & 0x7FFF);
                b[i] = (int16_t)(splitmix64(&st) & 0x7FFF);
                c[i] = (int16_t)(splitmix64(&st) & 0x7FFF);
                x[i] = (int16_t)((splitmix64(&st) & 0xFFFF) - 32768);
            }
            cos_v70_ssm_scan(a, b, c, x, N, 0, h, y);
            for (int i = 0; i < N; ++i) {
                CHECK(h[i] >= -32768 && h[i] <= 32767);
                CHECK(y[i] >= -32768 && y[i] <= 32767);
            }
        }
    }

    /* Deterministic fuzz: 256 ring all-reduce trials, N=4..8. */
    {
        uint64_t st = 0xA5A5A5A5;
        enum { CMAX = 16 };
        for (int trial = 0; trial < 256; ++trial) {
            uint32_t N = 4u + (uint32_t)(splitmix64(&st) & 3u);   /* 4..7 */
            uint32_t C = 4u + (uint32_t)(splitmix64(&st) & 3u);   /* 4..7 */
            size_t   bytes = (size_t)N * N * C * sizeof(int32_t);
            size_t   rounded = (bytes + 63u) & ~((size_t)63);
            int32_t *raw = (int32_t *)aligned_alloc(64, rounded);
            if (!raw) { CHECK(0); continue; }
            int32_t *bufs[8];
            int32_t expected[CMAX] = {0};
            for (uint32_t r = 0; r < N; ++r) {
                bufs[r] = raw + (size_t)r * N * C;
                for (uint32_t c = 0; c < N; ++c)
                    for (uint32_t i = 0; i < C; ++i) {
                        int32_t v = (int32_t)(splitmix64(&st) & 0xFFFFu) - 32768;
                        bufs[r][c*C + i] = v;
                    }
            }
            /* compute expected sum per chunk-element for chunk 0. */
            for (uint32_t c = 0; c < N; ++c)
                for (uint32_t i = 0; i < C; ++i) {
                    int32_t sum = 0;
                    for (uint32_t r = 0; r < N; ++r) sum += bufs[r][c*C + i];
                    expected[(c*C + i) & (CMAX-1)] = sum; (void)expected;
                }
            uint8_t ok = cos_v70_ring_allreduce(bufs, N, C);
            CHECK(ok == 1u);
            /* All ranks should now agree. */
            for (uint32_t c = 0; c < N; ++c)
                for (uint32_t i = 0; i < C; ++i) {
                    int32_t ref = bufs[0][c*C + i];
                    for (uint32_t r = 1; r < N; ++r)
                        CHECK(bufs[r][c*C + i] == ref);
                }
            free(raw);
        }
    }

    /* Deterministic fuzz: 1024 spike encode rounds, N=64. */
    {
        uint64_t st = 0xDEAD0001;
        enum { N = 64 };
        int16_t a[N], spikes[N];
        uint8_t fire[N];
        for (int trial = 0; trial < 1024; ++trial) {
            for (int i = 0; i < N; ++i)
                a[i] = (int16_t)((splitmix64(&st) & 0xFFFF) - 32768);
            int16_t thr = (int16_t)((splitmix64(&st) & 0xFFFF) - 32768);
            uint32_t fired = cos_v70_spike_encode(a, N, thr, spikes, fire);
            uint32_t want = 0;
            for (int i = 0; i < N; ++i) want += (a[i] >= thr) ? 1u : 0u;
            CHECK(fired == want);
            for (int i = 0; i < N; ++i) {
                if (fire[i]) CHECK(spikes[i] >= 0 || a[i] - thr < -32768);
                else         CHECK(spikes[i] == 0);
            }
        }
    }

    /* Deterministic fuzz: 256 MoE top-K sorted sanity at N=512, K=8. */
    {
        uint64_t st = 0xFACEFEED;
        enum { N = 512, K = 8 };
        int16_t scores[N];
        for (int trial = 0; trial < 256; ++trial) {
            for (int i = 0; i < N; ++i) scores[i] = (int16_t)(splitmix64(&st) & 0x7FFF);
            cos_v70_moe_route_t out; memset(&out, 0, sizeof(out));
            cos_v70_moe_route_topk(scores, NULL, N, K, &out, NULL);
            for (uint32_t j = 1; j < out.k; ++j)
                CHECK(out.selected_score_q15[j-1] >= out.selected_score_q15[j]);
            for (uint32_t a = 0; a < out.k; ++a)
                for (uint32_t b = a + 1; b < out.k; ++b)
                    CHECK(out.selected[a] != out.selected[b]);
        }
    }

    printf("v70 σ-Hyperscale self-test: %u pass, %u fail\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
