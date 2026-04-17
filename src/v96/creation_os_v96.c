/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v96 — σ-Diffusion self-test driver.
 */

#include "diffusion.h"

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
static void test_init(void)
{
    cos_v96_diffuser_t a, b;
    cos_v96_diffuser_init(&a);
    cos_v96_diffuser_init(&b);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V96_SENTINEL);
    CHECK(a.step == COS_V96_T);
    CHECK(a.monotone_violations == 0u);
    CHECK(a.identity_violations == 0u);
    CHECK(cos_v96_schedule_ok(&a) == 1u);
}

/* α_bar strictly monotonically decreasing. */
static void test_schedule_monotone(void)
{
    cos_v96_diffuser_t d;
    cos_v96_diffuser_init(&d);
    for (uint32_t t = 0u; t < COS_V96_T; ++t) {
        CHECK(d.alpha_bar[t + 1u] < d.alpha_bar[t]);
    }
    CHECK(d.alpha_bar[0]         == COS_V96_Q1);
    CHECK(d.alpha_bar[COS_V96_T] == 0);
}

/* Forward(t=0) gives exactly x0, Forward(t=T) gives exactly x1. */
static void test_forward_endpoints(uint64_t seed)
{
    cos_v96_diffuser_t d;
    cos_v96_diffuser_init(&d);
    cos_v96_q16_t x0[COS_V96_D], x1[COS_V96_D];
    uint64_t r = seed;
    for (uint32_t i = 0u; i < COS_V96_D; ++i) {
        x0[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
        x1[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
    }
    cos_v96_set_endpoints(&d, x0, x1);

    cos_v96_forward_to(&d, 0u);
    for (uint32_t i = 0u; i < COS_V96_D; ++i) CHECK(d.x[i] == x0[i]);

    cos_v96_forward_to(&d, COS_V96_T);
    for (uint32_t i = 0u; i < COS_V96_D; ++i) CHECK(d.x[i] == x1[i]);
}

/* forward(t) ∘ reverse_run(t) ≡ x0 within Q16.16 rounding (≤ 2 per dim). */
static void test_forward_reverse_identity(uint64_t seed)
{
    for (uint32_t trial = 0u; trial < 40u; ++trial) {
        cos_v96_diffuser_t d;
        cos_v96_diffuser_init(&d);
        uint64_t r = seed ^ (trial * 2654435761u);
        cos_v96_q16_t x0[COS_V96_D], x1[COS_V96_D];
        for (uint32_t i = 0u; i < COS_V96_D; ++i) {
            x0[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0x7FFFu));
            x1[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0x7FFFu));
        }
        cos_v96_set_endpoints(&d, x0, x1);

        /* Corrupt forward to t = T, then fully denoise. */
        cos_v96_forward_to(&d, COS_V96_T);
        cos_v96_reverse_run(&d, COS_V96_T);

        for (uint32_t i = 0u; i < COS_V96_D; ++i) {
            int32_t diff = (int32_t)d.x[i] - (int32_t)x0[i];
            if (diff < 0) diff = -diff;
            CHECK(diff <= 2);
        }
        CHECK(d.step == 0u);
    }
}

/* Energy (distance to x0) monotone non-increasing during reverse. */
static void test_energy_monotone(uint64_t seed)
{
    for (uint32_t trial = 0u; trial < 32u; ++trial) {
        cos_v96_diffuser_t d;
        cos_v96_diffuser_init(&d);
        uint64_t r = seed + trial * 131u;
        cos_v96_q16_t x0[COS_V96_D], x1[COS_V96_D];
        for (uint32_t i = 0u; i < COS_V96_D; ++i) {
            x0[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
            x1[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
        }
        cos_v96_set_endpoints(&d, x0, x1);
        cos_v96_forward_to(&d, COS_V96_T);
        int64_t prev = cos_v96_distance_to_x0(&d);
        for (uint32_t k = 0u; k < COS_V96_T; ++k) {
            cos_v96_reverse_step(&d);
            int64_t now = cos_v96_distance_to_x0(&d);
            CHECK(now <= prev);
            prev = now;
        }
        CHECK(d.monotone_violations == 0u);
        CHECK(cos_v96_ok(&d) == 1u);
    }
}

/* Determinism. */
static void test_determinism(uint64_t seed)
{
    for (uint32_t trial = 0u; trial < 16u; ++trial) {
        cos_v96_diffuser_t a, b;
        cos_v96_diffuser_init(&a);
        cos_v96_diffuser_init(&b);
        uint64_t r = seed ^ (trial * 97u);
        cos_v96_q16_t x0[COS_V96_D], x1[COS_V96_D];
        for (uint32_t i = 0u; i < COS_V96_D; ++i) {
            x0[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
            x1[i] = (cos_v96_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
        }
        cos_v96_set_endpoints(&a, x0, x1);
        cos_v96_set_endpoints(&b, x0, x1);
        for (uint32_t k = 0u; k < COS_V96_T; ++k) {
            cos_v96_reverse_step(&a);
            cos_v96_reverse_step(&b);
            CHECK(memcmp(&a, &b, sizeof(a)) == 0);
        }
    }
}

/* compose truth table (single AND). */
static void test_compose(void)
{
    CHECK(cos_v96_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v96_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v96_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v96_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v96 σ-Diffusion — integer rectified-flow / DDIM "
               "sampler (36-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0xD1FF5105EED96ULL;

    test_init();
    test_schedule_monotone();
    test_forward_endpoints(seed);
    test_forward_reverse_identity(seed);
    test_energy_monotone(seed);
    test_determinism(seed);
    test_compose();

    printf("v96 σ-Diffusion: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
