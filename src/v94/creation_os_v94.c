/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v94 — σ-Clifford self-test driver.
 *
 * Covers:
 *   - multiplication table correctness against textbook Cl(3,0)
 *   - associativity of the geometric product for random MVs
 *   - identity uv = u·v + u∧v
 *   - reverse involution:  ~~u = u
 *   - grade-k projector    orthogonality
 *   - equivariant layer output stays within clamp for random inputs
 *   - 34-bit compose truth table
 */

#include "clifford.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t pass_count = 0;
static uint32_t fail_count = 0;

#define CHECK(cond) do {                                                 \
    if (cond) { ++pass_count; }                                          \
    else {                                                               \
        ++fail_count;                                                    \
        if (fail_count <= 10u)                                           \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                    \
} while (0)

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

/* ------------------------------------------------------------------ */
/*  1. Textbook Cl(3,0) identities                                     */
/* ------------------------------------------------------------------ */
static void test_table(const cos_v94_algebra_t *a)
{
    /* e_i^2 = +1, indexed 1,2,4 (bitsets for e1,e2,e3). */
    for (uint32_t i = 1; i < COS_V94_DIM; i <<= 1) {
        CHECK(a->prod_idx [i][i] == 0u);
        CHECK(a->prod_sign[i][i] == 1);
    }
    /* e_1·e_2 = e_12, e_2·e_1 = -e_12. */
    CHECK(a->prod_idx [0b001][0b010] == 0b011u);
    CHECK(a->prod_sign[0b001][0b010] == 1);
    CHECK(a->prod_idx [0b010][0b001] == 0b011u);
    CHECK(a->prod_sign[0b010][0b001] == -1);
    /* e_1·e_2·e_3 = e_123 = bitset 0b111. */
    CHECK(a->prod_idx [0b011][0b100] == 0b111u);
    CHECK(a->prod_sign[0b011][0b100] == 1);
    /* Pseudo-scalar squared: I² = e_123 · e_123 = -1. */
    CHECK(a->prod_idx [0b111][0b111] == 0u);
    CHECK(a->prod_sign[0b111][0b111] == -1);
}

/* ------------------------------------------------------------------ */
/*  2. Associativity over random MVs                                   */
/* ------------------------------------------------------------------ */
static void test_assoc(const cos_v94_algebra_t *a, uint64_t seed)
{
    uint64_t s = seed;
    for (uint32_t trial = 0; trial < 200u; ++trial) {
        cos_v94_mv_t u, v, w, uv, vw, left, right;
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            u[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0x3FFu) - 0x200);
            v[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0x3FFu) - 0x200);
            w[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0x3FFu) - 0x200);
        }
        cos_v94_gp(a, u, v, uv);
        cos_v94_gp(a, v, w, vw);
        cos_v94_gp(a, uv, w, left);
        cos_v94_gp(a, u, vw, right);
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            int32_t d = (int32_t)left[i] - (int32_t)right[i];
            if (d < 0) d = -d;
            CHECK(d <= 2);            /* Q16.16 rounding */
        }
    }
}

/* ------------------------------------------------------------------ */
/*  3. uv = u·v + u∧v                                                  */
/* ------------------------------------------------------------------ */
static void test_inner_wedge(const cos_v94_algebra_t *a, uint64_t seed)
{
    uint64_t s = seed ^ 0xBEEFULL;
    for (uint32_t trial = 0; trial < 200u; ++trial) {
        cos_v94_mv_t u, v, uv, iv, wv;
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            u[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0x7FFu) - 0x400);
            v[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0x7FFu) - 0x400);
        }
        cos_v94_gp   (a, u, v, uv);
        cos_v94_inner(a, u, v, iv);
        cos_v94_wedge(a, u, v, wv);
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            /* Integer division by 2 in inner/wedge truncates each half
             * independently; re-summing 2·(iv+wv) must match (uv+vu)+(uv−vu)
             * = 2·uv exactly. */
            int32_t d2 = 2 * (int32_t)uv[i] - (2 * (int32_t)iv[i] + 2 * (int32_t)wv[i]);
            int32_t ad = (d2 < 0) ? -d2 : d2;
            CHECK(ad <= 2);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  4. Reverse involution                                              */
/* ------------------------------------------------------------------ */
static void test_reverse(uint64_t seed)
{
    uint64_t s = seed ^ 0xFEEDULL;
    for (uint32_t trial = 0; trial < 200u; ++trial) {
        cos_v94_mv_t u, ur, urr;
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            u[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
        }
        cos_v94_rev(u, ur);
        cos_v94_rev(ur, urr);
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) CHECK(urr[i] == u[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  5. Grade-k projector orthogonality: Σ_k <u>_k = u                  */
/* ------------------------------------------------------------------ */
static void test_grade_sum(uint64_t seed)
{
    uint64_t s = seed ^ 0xAAA5ULL;
    for (uint32_t trial = 0; trial < 100u; ++trial) {
        cos_v94_mv_t u, gk, sum = {0};
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            u[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
        }
        for (uint32_t k = 0; k <= 3u; ++k) {
            cos_v94_grade(u, k, gk);
            for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
                sum[i] = (cos_v94_q16_t)((int32_t)sum[i] + (int32_t)gk[i]);
            }
        }
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) CHECK(sum[i] == u[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  6. Layer stays clamped                                              */
/* ------------------------------------------------------------------ */
static void test_layer_clamp(const cos_v94_algebra_t *a, uint64_t seed)
{
    uint64_t s = seed ^ 0x1111ULL;
    for (uint32_t trial = 0; trial < 100u; ++trial) {
        cos_v94_mv_t u, v, b, y;
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            u[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
            v[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
            b[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0x1FFFu) - 0x1000);
        }
        cos_v94_layer(a, u, v, b, y);
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            CHECK(y[i] >= -(4 << 16) && y[i] <= (4 << 16));
        }
    }
}

/* ------------------------------------------------------------------ */
/*  7. Determinism                                                      */
/* ------------------------------------------------------------------ */
static void test_det(const cos_v94_algebra_t *a, uint64_t seed)
{
    uint64_t s = seed ^ 0xBEEBULL;
    for (uint32_t trial = 0; trial < 100u; ++trial) {
        cos_v94_mv_t u, v, y1, y2;
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
            u[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
            v[i] = (cos_v94_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
        }
        cos_v94_gp(a, u, v, y1);
        cos_v94_gp(a, u, v, y2);
        for (uint32_t i = 0; i < COS_V94_DIM; ++i) CHECK(y1[i] == y2[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  8. compose truth table                                             */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v94_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v94_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v94_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v94_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v94 σ-Clifford — Cl(3,0) geometric-algebra "
               "multivector algebra + equivariant layer "
               "(34-bit composed decision)\n");
        return 0;
    }

    cos_v94_algebra_t a;
    cos_v94_alg_init(&a);
    CHECK(cos_v94_ok(&a) == 1u);

    const uint64_t seed = 0xC11FF094C11FF094ULL;

    test_table(&a);
    test_assoc(&a, seed);
    test_inner_wedge(&a, seed);
    test_reverse(seed);
    test_grade_sum(seed);
    test_layer_clamp(&a, seed);
    test_det(&a, seed);
    test_compose();

    printf("v94 σ-Clifford: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
