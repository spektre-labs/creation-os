/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v100 — σ-Hyena self-test driver.
 */

#include "hyena.h"

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

static inline int32_t iabs(int32_t x) { return x < 0 ? -x : x; }

/* ------------------------------------------------------------------ */

static void test_init(void)
{
    cos_v100_hyena_t a, b;
    cos_v100_init(&a, 0xC0DECAFEu);
    cos_v100_init(&b, 0xC0DECAFEu);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V100_SENTINEL);
    CHECK(cos_v100_ok(&a) == 1u);
}

/* Filter envelope monotone: |h_{k+1}| ≤ |h_k|. */
static void test_filter_envelope(uint64_t seed)
{
    for (uint32_t trial = 0u; trial < 64u; ++trial) {
        cos_v100_hyena_t h;
        cos_v100_init(&h, seed ^ (trial * 131u));
        for (uint32_t k = 1u; k < COS_V100_L; ++k) {
            CHECK(iabs(h.h[k]) <= iabs(h.h[k - 1u]));
        }
    }
}

/* Causality: zeroing the input from position t0 onwards must leave
 * the first t0 outputs unchanged. */
static void test_causality(uint64_t seed)
{
    cos_v100_hyena_t h;
    cos_v100_init(&h, seed);

    cos_v100_q16_t x[COS_V100_L];
    uint64_t r = seed ^ 0xA5ULL;
    for (uint32_t i = 0u; i < COS_V100_L; ++i)
        x[i] = (cos_v100_q16_t)((int32_t)(xs(&r) & 0x3FFFu) - 0x2000);

    cos_v100_q16_t y_full[COS_V100_L];
    cos_v100_conv_only(&h, x, y_full);

    for (uint32_t t0 = 1u; t0 < COS_V100_L; ++t0) {
        cos_v100_q16_t x2[COS_V100_L];
        memcpy(x2, x, sizeof(x2));
        for (uint32_t i = t0; i < COS_V100_L; ++i) x2[i] = 0;

        cos_v100_q16_t y2[COS_V100_L];
        cos_v100_conv_only(&h, x2, y2);

        for (uint32_t t = 0u; t < t0; ++t) CHECK(y2[t] == y_full[t]);
    }
}

/* Linearity of the pre-gate conv: conv(x1 + x2) = conv(x1) + conv(x2). */
static void test_linearity(uint64_t seed)
{
    cos_v100_hyena_t h;
    cos_v100_init(&h, seed);

    for (uint32_t trial = 0u; trial < 64u; ++trial) {
        uint64_t r = seed + trial * 97u;
        cos_v100_q16_t x1[COS_V100_L], x2[COS_V100_L], x12[COS_V100_L];
        for (uint32_t i = 0u; i < COS_V100_L; ++i) {
            x1[i]  = (cos_v100_q16_t)((int32_t)(xs(&r) & 0x1FFFu) - 0x1000);
            x2[i]  = (cos_v100_q16_t)((int32_t)(xs(&r) & 0x1FFFu) - 0x1000);
            x12[i] = (cos_v100_q16_t)((int32_t)x1[i] + (int32_t)x2[i]);
        }
        cos_v100_q16_t y1[COS_V100_L], y2[COS_V100_L], y12[COS_V100_L];
        cos_v100_conv_only(&h, x1,  y1);
        cos_v100_conv_only(&h, x2,  y2);
        cos_v100_conv_only(&h, x12, y12);
        for (uint32_t t = 0u; t < COS_V100_L; ++t) {
            int32_t expected = (int32_t)y1[t] + (int32_t)y2[t];
            int32_t diff     = iabs((int32_t)y12[t] - expected);
            CHECK(diff <= 2);                           /* truncation */
        }
    }
}

/* Shift covariance: convolving a delta at position p gives a shifted
 * copy of the filter. */
static void test_shift_covariance(uint64_t seed)
{
    cos_v100_hyena_t h;
    cos_v100_init(&h, seed);
    for (uint32_t p = 0u; p < COS_V100_L; ++p) {
        cos_v100_q16_t x[COS_V100_L] = {0};
        x[p] = COS_V100_Q1;                             /* unit impulse */
        cos_v100_q16_t y[COS_V100_L];
        cos_v100_conv_only(&h, x, y);
        for (uint32_t t = 0u; t < COS_V100_L; ++t) {
            cos_v100_q16_t expected = (t < p) ? 0 : h.h[t - p];
            CHECK(y[t] == expected);
        }
    }
}

/* Gate bounded in [0, Q1] across init seeds. */
static void test_gate_bounds(uint64_t seed)
{
    for (uint32_t trial = 0u; trial < 64u; ++trial) {
        cos_v100_hyena_t h;
        cos_v100_init(&h, seed + trial);
        for (uint32_t t = 0u; t < COS_V100_L; ++t) {
            CHECK(h.g[t] >= 0);
            CHECK(h.g[t] <= (cos_v100_q16_t)COS_V100_Q1);
        }
        CHECK(cos_v100_ok(&h) == 1u);
    }
}

/* Apply = gate · conv_only (within clamp). */
static void test_apply_consistent(uint64_t seed)
{
    cos_v100_hyena_t h;
    cos_v100_init(&h, seed);
    for (uint32_t trial = 0u; trial < 32u; ++trial) {
        uint64_t r = seed + trial * 131u;
        cos_v100_q16_t x[COS_V100_L];
        for (uint32_t i = 0u; i < COS_V100_L; ++i)
            x[i] = (cos_v100_q16_t)((int32_t)(xs(&r) & 0x3FFFu) - 0x2000);
        cos_v100_q16_t y_pre[COS_V100_L], y_gated[COS_V100_L];
        cos_v100_conv_only(&h, x, y_pre);
        cos_v100_apply(&h, x, y_gated);
        for (uint32_t t = 0u; t < COS_V100_L; ++t) {
            int64_t expect = ((int64_t)h.g[t] * (int64_t)y_pre[t]) >> 16;
            if (expect >  (int64_t)0x7FFFFFFF) expect =  0x7FFFFFFF;
            if (expect < -(int64_t)0x7FFFFFFF) expect = -0x7FFFFFFF;
            /* Within clamp the two computations must agree. */
            if (iabs((int32_t)y_gated[t]) < (int32_t)(COS_V100_CLAMP - 1)) {
                CHECK((int32_t)y_gated[t] == (int32_t)expect);
            }
        }
    }
}

/* Determinism. */
static void test_determinism(uint64_t seed)
{
    for (uint32_t trial = 0u; trial < 128u; ++trial) {
        cos_v100_hyena_t a, b;
        cos_v100_init(&a, seed ^ (trial * 97u));
        cos_v100_init(&b, seed ^ (trial * 97u));
        uint64_t r = seed + trial * 17u;
        cos_v100_q16_t x[COS_V100_L];
        for (uint32_t i = 0u; i < COS_V100_L; ++i)
            x[i] = (cos_v100_q16_t)((int32_t)(xs(&r) & 0x1FFFu) - 0x1000);
        cos_v100_q16_t ya[COS_V100_L], yb[COS_V100_L];
        cos_v100_apply(&a, x, ya);
        cos_v100_apply(&b, x, yb);
        CHECK(memcmp(ya, yb, sizeof(ya)) == 0);
        CHECK(memcmp(&a,  &b,  sizeof(a))  == 0);
    }
}

static void test_compose(void)
{
    CHECK(cos_v100_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v100_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v100_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v100_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v100 σ-Hyena — sub-quadratic gated long "
               "convolution operator (40-bit composed decision)\n");
        return 0;
    }
    const uint64_t seed = 0x100A0071E500ULL;

    test_init();
    test_filter_envelope(seed);
    test_causality(seed);
    test_linearity(seed);
    test_shift_covariance(seed);
    test_gate_bounds(seed);
    test_apply_consistent(seed);
    test_determinism(seed);
    test_compose();

    printf("v100 σ-Hyena: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
