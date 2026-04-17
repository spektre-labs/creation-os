/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v66 — σ-Silicon self-test and microbench driver.
 *
 *   ./creation_os_v66                 run self-tests
 *   ./creation_os_v66 --self-test     same
 *   ./creation_os_v66 --bench         microbench (gemv-i8, gemv-ternary, ntw, hsl)
 *   ./creation_os_v66 --version       print v66 version
 *   ./creation_os_v66 --features      print detected CPU feature bits
 *   ./creation_os_v66 --decision v60 v61 v62 v63 v64 v65 v66
 *                                     emit 7-bit JSON verdict
 */

#include "silicon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

static uint32_t g_pass = 0;
static uint32_t g_fail = 0;

static void ck(int cond, const char *name)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        fprintf(stderr, "FAIL: %s\n", name);
    }
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint64_t splitmix64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static void *alloc64(size_t n)
{
    size_t pad = (n + 63u) & ~((size_t)63u);
    void *p = aligned_alloc(64, pad);
    if (p) memset(p, 0, pad);
    return p;
}

/* ================================================================
 *  1. Seven-bit composition truth table.
 *     128 rows × 8 assertions = 1024 assertions.
 * ================================================================ */
static void test_composition(void)
{
    for (uint32_t row = 0; row < 128u; ++row) {
        uint8_t a = (row >> 0) & 1;
        uint8_t b = (row >> 1) & 1;
        uint8_t c = (row >> 2) & 1;
        uint8_t d = (row >> 3) & 1;
        uint8_t e = (row >> 4) & 1;
        uint8_t f = (row >> 5) & 1;
        uint8_t g = (row >> 6) & 1;
        cos_v66_decision_t v =
            cos_v66_compose_decision(a, b, c, d, e, f, g);
        ck(v.v60_ok == a, "compose v60_ok");
        ck(v.v61_ok == b, "compose v61_ok");
        ck(v.v62_ok == c, "compose v62_ok");
        ck(v.v63_ok == d, "compose v63_ok");
        ck(v.v64_ok == e, "compose v64_ok");
        ck(v.v65_ok == f, "compose v65_ok");
        ck(v.v66_ok == g, "compose v66_ok");
        uint8_t expect = a & b & c & d & e & f & g;
        ck(v.allow == expect, "compose allow");
    }
    /* Non-zero high bits collapse to 1. */
    cos_v66_decision_t v = cos_v66_compose_decision(7, 9, 11, 13, 255, 1, 99);
    ck(v.v60_ok == 1 && v.v61_ok == 1 && v.v62_ok == 1 &&
       v.v63_ok == 1 && v.v64_ok == 1 && v.v65_ok == 1 && v.v66_ok == 1,
       "compose: high bits collapse");
    ck(v.allow == 1, "compose: collapsed = ALLOW");

    /* Single-zero must deny. */
    for (int z = 0; z < 7; ++z) {
        uint8_t in[7] = {1,1,1,1,1,1,1};
        in[z] = 0;
        cos_v66_decision_t w = cos_v66_compose_decision(
            in[0], in[1], in[2], in[3], in[4], in[5], in[6]);
        ck(w.allow == 0, "compose: any zero → DENY");
    }
}

/* ================================================================
 *  2. Feature detection.
 * ================================================================ */
static void test_features(void)
{
    uint32_t f1 = cos_v66_features();
    uint32_t f2 = cos_v66_features();
    ck(f1 == f2, "features: cached, deterministic");

    char buf[128];
    size_t n = cos_v66_features_describe(buf, sizeof(buf));
    ck(n > 0,             "features: describe non-empty");
    ck(buf[n] == '\0',    "features: NUL-terminated");

    /* Truncation: tiny buffer must still terminate. */
    char tiny[4];
    size_t m = cos_v66_features_describe(tiny, sizeof(tiny));
    ck(tiny[m] == '\0', "features: truncated NUL");

    /* Zero capacity is a no-op. */
    char z[1];
    size_t k = cos_v66_features_describe(z, 0);
    ck(k == 0, "features: zero-cap → 0 bytes");
}

/* ================================================================
 *  3. INT8 GEMV.
 *     Bit-identical NEON vs scalar reference on 32 random shapes.
 * ================================================================ */
static int32_t ref_dot(const int8_t *w, const int8_t *x, uint32_t n)
{
    int32_t a = 0;
    for (uint32_t i = 0; i < n; ++i) a += (int32_t)w[i] * (int32_t)x[i];
    return a;
}

static int16_t ref_sat(int32_t v)
{
    if (v >  32767) v =  32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

static void test_gemv_int8(void)
{
    uint64_t s = 0xDEADBEEFCAFEBABEULL;

    /* Bad-input rejects. */
    int16_t y_dummy[1] = {0};
    ck(cos_v66_gemv_int8(NULL, NULL, NULL, y_dummy, 1, 16) == -1, "gemv-i8: NULL W");
    ck(cos_v66_gemv_int8((const int8_t *)"x", NULL, NULL, y_dummy, 1, 16) == -1, "gemv-i8: NULL x");
    int8_t Wd[16] = {0}, xd[16] = {0};
    ck(cos_v66_gemv_int8(Wd, xd, NULL, y_dummy, 1, 0) == -1, "gemv-i8: cols=0");
    ck(cos_v66_gemv_int8(Wd, xd, NULL, y_dummy, 1, 17) == -1, "gemv-i8: cols%16!=0");

    /* 32 random shapes. */
    for (int trial = 0; trial < 32; ++trial) {
        uint32_t rows = 1u + (uint32_t)(splitmix64(&s) % 16u);
        uint32_t cols = 16u * (1u + (uint32_t)(splitmix64(&s) % 16u));

        int8_t  *W    = (int8_t *)alloc64((size_t)rows * cols);
        int8_t  *x    = (int8_t *)alloc64(cols);
        int32_t *bias = (int32_t *)alloc64((size_t)rows * sizeof(int32_t));
        int16_t *y    = (int16_t *)alloc64((size_t)rows * sizeof(int16_t));

        for (uint32_t i = 0; i < rows * cols; ++i)
            W[i] = (int8_t)(splitmix64(&s) & 0xFFu);
        for (uint32_t i = 0; i < cols; ++i)
            x[i] = (int8_t)(splitmix64(&s) & 0xFFu);
        for (uint32_t i = 0; i < rows; ++i)
            bias[i] = (int32_t)(splitmix64(&s) & 0x3FFu) - 512;

        int rc = cos_v66_gemv_int8(W, x, bias, y, rows, cols);
        ck(rc == 0, "gemv-i8: returns 0");

        int all_match = 1;
        for (uint32_t m = 0; m < rows; ++m) {
            int32_t r = ref_dot(W + (size_t)m * cols, x, cols) + bias[m];
            int16_t want = ref_sat(r);
            if (y[m] != want) { all_match = 0; break; }
        }
        ck(all_match, "gemv-i8: NEON == scalar");

        /* Saturation sanity: extreme inputs. */
        for (uint32_t i = 0; i < rows * cols; ++i) W[i] = 127;
        for (uint32_t i = 0; i < cols;        ++i) x[i] = 127;
        for (uint32_t i = 0; i < rows;        ++i) bias[i] = 0;
        rc = cos_v66_gemv_int8(W, x, bias, y, rows, cols);
        ck(rc == 0,            "gemv-i8: rerun ok");
        ck(y[0] == 32767,      "gemv-i8: saturate +max");

        for (uint32_t i = 0; i < rows * cols; ++i) W[i] = -128;
        for (uint32_t i = 0; i < cols;        ++i) x[i] = 127;
        rc = cos_v66_gemv_int8(W, x, bias, y, rows, cols);
        ck(rc == 0,            "gemv-i8: rerun ok 2");
        ck(y[0] == -32768,     "gemv-i8: saturate -max");

        free(W); free(x); free(bias); free(y);
    }
}

/* ================================================================
 *  4. Ternary GEMV.
 * ================================================================ */
static void test_gemv_ternary(void)
{
    uint64_t s = 0xC0FFEE12ULL;

    int16_t y_dummy[1] = {0};
    ck(cos_v66_gemv_ternary(NULL, NULL, NULL, y_dummy, 1, 4) == -1, "gemv-t: NULL");
    uint8_t Wd[4] = {0}; int8_t xd[16] = {0};
    ck(cos_v66_gemv_ternary(Wd, xd, NULL, y_dummy, 1, 0) == -1, "gemv-t: cols=0");
    ck(cos_v66_gemv_ternary(Wd, xd, NULL, y_dummy, 1, 5) == -1, "gemv-t: cols%4!=0");

    for (int trial = 0; trial < 32; ++trial) {
        uint32_t rows = 1u + (uint32_t)(splitmix64(&s) % 8u);
        uint32_t cols = 4u * (1u + (uint32_t)(splitmix64(&s) % 32u));

        size_t   rb = cols / 4u;
        uint8_t *W  = (uint8_t *)alloc64((size_t)rows * rb);
        int8_t  *x  = (int8_t  *)alloc64(cols);
        int16_t *y  = (int16_t *)alloc64((size_t)rows * sizeof(int16_t));

        /* Random ternary weights. */
        for (uint32_t r = 0; r < rows; ++r) {
            for (uint32_t b = 0; b < rb; ++b) {
                uint64_t u = splitmix64(&s);
                uint8_t  byte = 0;
                for (uint32_t k = 0; k < 4; ++k) {
                    uint8_t code = (uint8_t)(u & 0x3u);
                    if (code == 3u) code = 0u;
                    byte |= (uint8_t)(code << (k * 2));
                    u >>= 2;
                }
                W[r * rb + b] = byte;
            }
        }
        for (uint32_t i = 0; i < cols; ++i)
            x[i] = (int8_t)(splitmix64(&s) & 0xFFu);

        int rc = cos_v66_gemv_ternary(W, x, NULL, y, rows, cols);
        ck(rc == 0, "gemv-t: returns 0");

        int ok = 1;
        for (uint32_t m = 0; m < rows; ++m) {
            int32_t acc = 0;
            for (uint32_t b = 0; b < rb; ++b) {
                uint8_t pk = W[m * rb + b];
                int8_t  q[4]; static const int8_t T[4] = { 0, +1, -1, 0 };
                q[0] = T[(pk >> 0) & 0x3u];
                q[1] = T[(pk >> 2) & 0x3u];
                q[2] = T[(pk >> 4) & 0x3u];
                q[3] = T[(pk >> 6) & 0x3u];
                acc += (int32_t)q[0] * (int32_t)x[b * 4 + 0];
                acc += (int32_t)q[1] * (int32_t)x[b * 4 + 1];
                acc += (int32_t)q[2] * (int32_t)x[b * 4 + 2];
                acc += (int32_t)q[3] * (int32_t)x[b * 4 + 3];
            }
            if (y[m] != ref_sat(acc)) { ok = 0; break; }
        }
        ck(ok, "gemv-t: matches scalar reference");

        /* Defensive: 11-codes get treated as 0. */
        memset(W, 0xFF, (size_t)rows * rb);   /* every weight = code 11 */
        rc = cos_v66_gemv_ternary(W, x, NULL, y, rows, cols);
        ck(rc == 0,           "gemv-t: ff returns 0");
        ck(y[0] == 0,         "gemv-t: code 11 → 0 weight");

        free(W); free(x); free(y);
    }
}

/* ================================================================
 *  5. NativeTernary wire round-trip.
 * ================================================================ */
static void test_ntw(void)
{
    uint64_t s = 0xABCDEF01ULL;
    for (int trial = 0; trial < 64; ++trial) {
        uint32_t n = 1u + (uint32_t)(splitmix64(&s) % 256u);
        int8_t  *src = (int8_t  *)alloc64(n);
        for (uint32_t i = 0; i < n; ++i) {
            uint64_t u = splitmix64(&s) % 3u;
            src[i] = (int8_t)((u == 0) ? 0 : (u == 1 ? 1 : -1));
        }
        uint32_t cap = (n + 3u) / 4u;
        uint8_t *wire = (uint8_t *)alloc64(cap);
        uint8_t *pk   = (uint8_t *)alloc64(cap);

        int32_t enc = cos_v66_ntw_encode(src, n, wire, cap);
        ck(enc == (int32_t)cap, "ntw: encoded length matches");

        int32_t dec = cos_v66_ntw_decode(wire, cap, pk, n);
        ck(dec == (int32_t)n, "ntw: decoded count matches");

        /* Decode the packed-2-bit form and compare. */
        int ok = 1;
        for (uint32_t i = 0; i < n; ++i) {
            uint8_t code = (uint8_t)((pk[i >> 2] >> ((i & 3u) * 2u)) & 0x3u);
            int8_t  v = (code == 0) ? 0 : (code == 1 ? +1 : (code == 2 ? -1 : 0));
            if (v != src[i]) { ok = 0; break; }
        }
        ck(ok, "ntw: round-trip integrity");

        /* Truncated wire must fail. */
        if (cap > 0) {
            int32_t bad = cos_v66_ntw_decode(wire, cap - 1u, pk, n);
            ck(bad == -1, "ntw: truncated wire → -1");
        }

        free(src); free(wire); free(pk);
    }

    /* Encode capacity rejection. */
    int8_t v[4] = { 0, 1, -1, 0 };
    uint8_t out;
    int32_t bad = cos_v66_ntw_encode(v, 4, &out, 0);
    ck(bad == -1, "ntw: encode cap=0 → -1");
}

/* ================================================================
 *  6. Conformal abstention gate.
 * ================================================================ */
static void test_conformal(void)
{
    cos_v66_conformal_t *g = cos_v66_conformal_new(8, /*tau=*/16384,
                                                    /*eps=*/3277);
    ck(g != NULL, "cfc: alloc");
    ck(cos_v66_conformal_new(0, 0, 0) == NULL, "cfc: zero groups → NULL");

    /* Initial gate: score above τ in any group passes. */
    ck(cos_v66_conformal_gate(g, 0, 30000) == 1, "cfc: high score allows");
    ck(cos_v66_conformal_gate(g, 0,   100) == 0, "cfc: low score denies");

    /* Out-of-range group → strict deny. */
    ck(cos_v66_conformal_gate(g, 99, 30000) == 0, "cfc: bad group → DENY");

    /* Streaming update: feed many factual high-scores into group 1.
     * Threshold should relax (drop) toward 0; gate eventually allows
     * mid-range. */
    for (int i = 0; i < 200; ++i)
        cos_v66_conformal_update(g, 1, 20000, 1);
    ck(g->thr_q15[1] <= 16384, "cfc: factual stream → relaxed thr");

    /* Feed many miscoverage events into group 2; threshold tightens. */
    for (int i = 0; i < 200; ++i)
        cos_v66_conformal_update(g, 2, 5000, 0);
    ck(g->thr_q15[2] >= 16384, "cfc: miscov stream → tightened thr");

    /* Monotone: doubling the score never reduces the gate decision. */
    for (int i = 0; i < 32; ++i) {
        int16_t s_low  = (int16_t)(1000 * i);
        int16_t s_high = (int16_t)(s_low * 2 < 32767 ? s_low * 2 : 32767);
        uint8_t lo = cos_v66_conformal_gate(g, 0, s_low);
        uint8_t hi = cos_v66_conformal_gate(g, 0, s_high);
        ck(hi >= lo, "cfc: monotone in score");
    }

    /* NULL gate is safe. */
    ck(cos_v66_conformal_gate(NULL, 0, 30000) == 0, "cfc: NULL → DENY");

    cos_v66_conformal_free(g);
    cos_v66_conformal_free(NULL);
}

/* ================================================================
 *  7. HSL — bytecode interpreter.
 * ================================================================ */
static void test_hsl(void)
{
    cos_v66_hsl_state_t *s = cos_v66_hsl_new(4);
    ck(s != NULL, "hsl: alloc");
    ck(cos_v66_hsl_new(0) == NULL, "hsl: nreg=0 → NULL");
    ck(cos_v66_hsl_new(99) == NULL, "hsl: nreg too large → NULL");

    /* Set up arena: one 8x16 INT8 weight matrix, identity-like. */
    uint32_t rows = 8, cols = 16;
    int8_t  *W    = (int8_t *)alloc64((size_t)rows * cols);
    int8_t  *x    = (int8_t *)alloc64(cols);
    int16_t *y    = (int16_t *)alloc64((size_t)rows * sizeof(int16_t));
    for (uint32_t r = 0; r < rows; ++r)
        for (uint32_t c = 0; c < cols; ++c)
            W[r * cols + c] = (r == c) ? (int8_t)1 : (int8_t)0;
    for (uint32_t c = 0; c < cols; ++c) x[c] = (int8_t)(c + 1);

    const int8_t  *w_slots[1] = { W };
    const uint32_t w_rows[1] = { rows };
    const uint32_t w_cols[1] = { cols };
    const int8_t  *x_slots[1] = { x };
    int16_t       *y_slots[1] = { y };

    cos_v66_conformal_t *cg = cos_v66_conformal_new(4, /*tau=*/0, /*eps=*/3277);

    cos_v66_inst_t prog[] = {
        { .op = COS_V66_OP_LOAD,    .rd = 0, .a = 1000, .b = 1, .c = 0 },
        { .op = COS_V66_OP_GEMV_I8, .rd = 1, .a = 0,    .b = 0, .c = 0 },
        { .op = COS_V66_OP_CMPGE,   .rd = 1, .a = 0,    .b = 0, .c = 0 },
        { .op = COS_V66_OP_ABSTAIN, .rd = 2, .a = 0,    .b = 30000, .c = 0 },
        { .op = COS_V66_OP_GATE,    .rd = 0, .a = 0,    .b = 0, .c = 0 },
        { .op = COS_V66_OP_HALT,    .rd = 0, .a = 0,    .b = 0, .c = 0 },
    };

    int rc = cos_v66_hsl_exec(s, prog, sizeof(prog) / sizeof(prog[0]),
                              w_slots, w_rows, w_cols, x_slots,
                              NULL, NULL, NULL, y_slots,
                              cg, /*budget=*/ (uint64_t)1 << 30);
    ck(rc == 0,                        "hsl: exec ok");
    ck(cos_v66_hsl_v66_ok(s) == 1,     "hsl: gate passes under budget");
    ck(cos_v66_hsl_cost(s) > 0,        "hsl: cost was charged");
    ck(cos_v66_hsl_gate_bit(s) == 1,   "hsl: cmpge set gate_bit");

    /* Tiny budget → over-budget, v66_ok = 0. */
    cos_v66_hsl_reset(s);
    rc = cos_v66_hsl_exec(s, prog, sizeof(prog) / sizeof(prog[0]),
                          w_slots, w_rows, w_cols, x_slots,
                          NULL, NULL, NULL, y_slots,
                          cg, /*budget=*/ 4u);
    ck(rc == 0,                        "hsl: over-budget early return ok");
    ck(cos_v66_hsl_v66_ok(s) == 0,     "hsl: over-budget → v66_ok=0");

    /* Malformed opcode. */
    cos_v66_inst_t bad[] = {
        { .op = 99, .rd = 0, .a = 0, .b = 0, .c = 0 },
    };
    rc = cos_v66_hsl_exec(s, bad, 1, w_slots, w_rows, w_cols, x_slots,
                          NULL, NULL, NULL, y_slots,
                          cg, (uint64_t)1 << 30);
    ck(rc == -1, "hsl: malformed op → -1");

    /* Out-of-range register. */
    cos_v66_inst_t oor[] = {
        { .op = COS_V66_OP_LOAD, .rd = 99, .a = 0, .b = 0, .c = 0 },
    };
    rc = cos_v66_hsl_exec(s, oor, 1, w_slots, w_rows, w_cols, x_slots,
                          NULL, NULL, NULL, y_slots,
                          cg, (uint64_t)1 << 30);
    ck(rc == -1, "hsl: oor register → -1");

    /* Multiple programs share a state cleanly. */
    for (int i = 0; i < 16; ++i) {
        cos_v66_hsl_reset(s);
        rc = cos_v66_hsl_exec(s, prog, sizeof(prog) / sizeof(prog[0]),
                              w_slots, w_rows, w_cols, x_slots,
                              NULL, NULL, NULL, y_slots,
                              cg, (uint64_t)1 << 30);
        ck(rc == 0,                    "hsl: idempotent reset");
        ck(cos_v66_hsl_v66_ok(s) == 1, "hsl: idempotent v66_ok");
    }

    cos_v66_conformal_free(cg);
    cos_v66_hsl_free(s);
    cos_v66_hsl_free(NULL);
    free(W); free(x); free(y);
}

/* ================================================================
 *  Top-level driver.
 * ================================================================ */
static int run_self_test(void)
{
    test_composition();
    test_features();
    test_gemv_int8();
    test_gemv_ternary();
    test_ntw();
    test_conformal();
    test_hsl();
    printf("v66 self-test: %u pass, %u fail\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

/* ---------------- microbench ------------------------------------ */

static void bench_gemv_int8(void)
{
    const uint32_t rows = 256, cols = 1024;
    int8_t  *W = (int8_t *)alloc64((size_t)rows * cols);
    int8_t  *x = (int8_t *)alloc64(cols);
    int16_t *y = (int16_t *)alloc64((size_t)rows * sizeof(int16_t));

    uint64_t s = 0x123456789ULL;
    for (uint32_t i = 0; i < rows * cols; ++i) W[i] = (int8_t)(splitmix64(&s) & 0xFFu);
    for (uint32_t i = 0; i < cols;        ++i) x[i] = (int8_t)(splitmix64(&s) & 0xFFu);

    const uint32_t N = 200;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i)
        (void)cos_v66_gemv_int8(W, x, NULL, y, rows, cols);
    uint64_t dt = now_ns() - t0;
    double   sec = (double)dt / 1e9;
    double   gops = (double)N * rows * cols / sec / 1e9;
    printf("  gemv-int8 (256x1024): %9.0f calls/s (%.2f Gops/s)\n",
           (double)N / sec, gops);

    free(W); free(x); free(y);
}

static void bench_gemv_ternary(void)
{
    const uint32_t rows = 512, cols = 1024;
    uint8_t *W = (uint8_t *)alloc64((size_t)rows * cols / 4u);
    int8_t  *x = (int8_t  *)alloc64(cols);
    int16_t *y = (int16_t *)alloc64((size_t)rows * sizeof(int16_t));
    uint64_t s = 0xC0FFEE10ULL;
    for (uint32_t i = 0; i < rows * cols / 4u; ++i)
        W[i] = (uint8_t)(splitmix64(&s) & 0x55u);  /* keep codes in {0,1} */
    for (uint32_t i = 0; i < cols; ++i)
        x[i] = (int8_t)(splitmix64(&s) & 0xFFu);

    const uint32_t N = 200;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i)
        (void)cos_v66_gemv_ternary(W, x, NULL, y, rows, cols);
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    double gops = (double)N * rows * cols / sec / 1e9;
    printf("  gemv-ternary (512x1024): %9.0f calls/s (%.2f Gops/s)\n",
           (double)N / sec, gops);

    free(W); free(x); free(y);
}

static void bench_ntw(void)
{
    const uint32_t N_W = 1u << 16;
    int8_t *src = (int8_t *)alloc64(N_W);
    uint8_t *wire = (uint8_t *)alloc64((N_W + 3u) / 4u);
    uint8_t *pk   = (uint8_t *)alloc64((N_W + 3u) / 4u);
    uint64_t s = 0xABCDULL;
    for (uint32_t i = 0; i < N_W; ++i) {
        uint64_t u = splitmix64(&s) % 3u;
        src[i] = (int8_t)((u == 0) ? 0 : (u == 1 ? 1 : -1));
    }
    (void)cos_v66_ntw_encode(src, N_W, wire, (N_W + 3u) / 4u);

    const uint32_t N = 1000;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i)
        (void)cos_v66_ntw_decode(wire, (N_W + 3u) / 4u, pk, N_W);
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    double bytes = (double)N * (double)((N_W + 3u) / 4u);
    printf("  ntw decode: %9.0f calls/s (%.2f MB/s)\n",
           (double)N / sec, bytes / sec / 1e6);

    free(src); free(wire); free(pk);
}

static void bench_hsl(void)
{
    cos_v66_hsl_state_t *st = cos_v66_hsl_new(4);
    cos_v66_conformal_t *cg = cos_v66_conformal_new(4, 0, 3277);
    uint32_t rows = 8, cols = 16;
    int8_t  *W = (int8_t *)alloc64((size_t)rows * cols);
    int8_t  *x = (int8_t *)alloc64(cols);
    int16_t *y = (int16_t *)alloc64((size_t)rows * sizeof(int16_t));
    for (uint32_t r = 0; r < rows; ++r)
        for (uint32_t c = 0; c < cols; ++c)
            W[r * cols + c] = (r == c) ? 1 : 0;
    for (uint32_t c = 0; c < cols; ++c) x[c] = (int8_t)(c + 1);
    const int8_t  *ws[1] = { W }; const uint32_t wr[1] = { rows };
    const uint32_t wc[1] = { cols }; const int8_t *xs[1] = { x };
    int16_t *ys[1] = { y };

    cos_v66_inst_t prog[] = {
        { .op = COS_V66_OP_LOAD,    .rd = 0, .a = 1000, .b = 1, .c = 0 },
        { .op = COS_V66_OP_GEMV_I8, .rd = 1, .a = 0,    .b = 0, .c = 0 },
        { .op = COS_V66_OP_CMPGE,   .rd = 1, .a = 0,    .b = 0, .c = 0 },
        { .op = COS_V66_OP_GATE,    .rd = 0, .a = 0,    .b = 0, .c = 0 },
        { .op = COS_V66_OP_HALT,    .rd = 0, .a = 0,    .b = 0, .c = 0 },
    };
    const uint32_t N = 50000;
    uint64_t t0 = now_ns();
    for (uint32_t i = 0; i < N; ++i)
        (void)cos_v66_hsl_exec(st, prog, sizeof(prog)/sizeof(prog[0]),
                               ws, wr, wc, xs, NULL, NULL, NULL, ys,
                               cg, (uint64_t)1 << 30);
    uint64_t dt = now_ns() - t0;
    double sec = (double)dt / 1e9;
    printf("  hsl (5-op prog): %9.0f progs/s (%.2f M ops/s)\n",
           (double)N / sec, (double)N * 5.0 / sec / 1e6);
    cos_v66_conformal_free(cg);
    cos_v66_hsl_free(st);
    free(W); free(x); free(y);
}

static int run_bench(void)
{
    char fbuf[64];
    cos_v66_features_describe(fbuf, sizeof(fbuf));
    printf("v66 σ-Silicon microbench (features: %s):\n", fbuf);
    bench_gemv_int8();
    bench_gemv_ternary();
    bench_ntw();
    bench_hsl();
    return 0;
}

/* ---------------- main ------------------------------------------ */

static int cmd_decision(int argc, char **argv)
{
    if (argc < 9) {
        fprintf(stderr,
                "usage: %s --decision v60 v61 v62 v63 v64 v65 v66\n",
                argv[0]);
        return 2;
    }
    uint8_t lanes[7];
    for (int i = 0; i < 7; ++i) {
        long v = strtol(argv[2 + i], NULL, 10);
        lanes[i] = (v != 0) ? 1u : 0u;
    }
    cos_v66_decision_t d = cos_v66_compose_decision(
        lanes[0], lanes[1], lanes[2], lanes[3], lanes[4], lanes[5], lanes[6]);
    int reason = (d.v60_ok << 0) | (d.v61_ok << 1) | (d.v62_ok << 2) |
                 (d.v63_ok << 3) | (d.v64_ok << 4) | (d.v65_ok << 5) |
                 (d.v66_ok << 6);
    printf("{\"allow\":%u,\"reason\":%d,"
           "\"v60_ok\":%u,\"v61_ok\":%u,\"v62_ok\":%u,"
           "\"v63_ok\":%u,\"v64_ok\":%u,\"v65_ok\":%u,\"v66_ok\":%u}\n",
           d.allow, reason,
           d.v60_ok, d.v61_ok, d.v62_ok,
           d.v63_ok, d.v64_ok, d.v65_ok, d.v66_ok);
    return d.allow ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "--version") == 0) {
        printf("%s\n", cos_v66_version);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--features") == 0) {
        char buf[128];
        cos_v66_features_describe(buf, sizeof(buf));
        printf("%s\n", buf);
        return 0;
    }
    if (argc >= 2 && strcmp(argv[1], "--bench") == 0) {
        return run_bench();
    }
    if (argc >= 2 && strcmp(argv[1], "--decision") == 0) {
        return cmd_decision(argc, argv);
    }
    return run_self_test();
}
