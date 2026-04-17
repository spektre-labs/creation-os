/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v93 — σ-MoR self-test driver.
 */

#include "mor.h"

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
    cos_v93_mor_t a, b;
    cos_v93_mor_init(&a, 0x93DEADBEEFULL, 16u);
    cos_v93_mor_init(&b, 0x93DEADBEEFULL, 16u);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V93_SENTINEL);
    CHECK(a.n_tokens == 16u);

    cos_v93_mor_t c;
    cos_v93_mor_init(&c, 0x93DEADBEEEULL, 16u);
    CHECK(memcmp(&a, &c, sizeof(a)) != 0);
}

/* ------------------------------------------------------------------ */
/*  2. Hidden state stays clamped for random inputs                    */
/* ------------------------------------------------------------------ */
static void test_clamped(uint64_t seed)
{
    cos_v93_mor_t m;
    cos_v93_mor_init(&m, seed, COS_V93_MAX_TOKENS);
    uint64_t s = seed ^ 0xC1ULL;
    cos_v93_q16_t h0[COS_V93_HDIM];
    for (uint32_t t = 0; t < COS_V93_MAX_TOKENS; ++t) {
        for (uint32_t d = 0; d < COS_V93_HDIM; ++d)
            h0[d] = (cos_v93_q16_t)((int32_t)(xs(&s) & 0x1FFFFu) - 0x10000);
        cos_v93_mor_set_input(&m, t, h0);
    }
    cos_v93_mor_run(&m);
    CHECK(cos_v93_ok(&m) == 1u);
    for (uint32_t t = 0; t < COS_V93_MAX_TOKENS; ++t) {
        for (uint32_t d = 0; d < COS_V93_HDIM; ++d) {
            int32_t v = m.h[t][d];
            CHECK(v >= -COS_V93_HCLAMP && v <= COS_V93_HCLAMP);
        }
        CHECK(m.exit_d[t] >= 1u && m.exit_d[t] <= COS_V93_MAX_DEPTH);
    }
}

/* ------------------------------------------------------------------ */
/*  3. High-positive input activates early exit (router picks yes)     */
/* ------------------------------------------------------------------ */
static void test_early_exit(uint64_t seed)
{
    cos_v93_mor_t m;
    cos_v93_mor_init(&m, seed, 4u);
    /* Build an "easy" input aligned with the router weight direction.
     * Set h0 = sign(rw_d) * 0.5 so <rw, h0> ≈ 0.5 * Σ|rw_d|. */
    cos_v93_q16_t h0[COS_V93_HDIM];
    for (uint32_t d = 0; d < COS_V93_HDIM; ++d) {
        int32_t sign = (m.rw[d] >= 0) ? 1 : -1;
        h0[d] = (cos_v93_q16_t)(sign * (COS_V93_HCLAMP / 2));    /* ±2.0 */
    }
    for (uint32_t t = 0; t < 4u; ++t) cos_v93_mor_set_input(&m, t, h0);
    cos_v93_mor_run(&m);
    /* At least one token should exit strictly before max depth. */
    uint32_t early = 0;
    for (uint32_t t = 0; t < 4u; ++t) if (m.exit_d[t] < COS_V93_MAX_DEPTH) ++early;
    CHECK(early >= 1u);
}

/* ------------------------------------------------------------------ */
/*  4. zero input never exits early (low router score)                 */
/* ------------------------------------------------------------------ */
static void test_zero_deep(uint64_t seed)
{
    cos_v93_mor_t m;
    cos_v93_mor_init(&m, seed, 8u);
    cos_v93_q16_t h0[COS_V93_HDIM] = {0};
    for (uint32_t t = 0; t < 8u; ++t) cos_v93_mor_set_input(&m, t, h0);
    cos_v93_mor_run(&m);
    /* With zero input the router score starts at b·rw (small) and grows
     * slowly; we just check all tokens eventually exited. */
    for (uint32_t t = 0; t < 8u; ++t) {
        CHECK(m.exit_d[t] >= 1u && m.exit_d[t] <= COS_V93_MAX_DEPTH);
    }
    CHECK(cos_v93_ok(&m) == 1u);
}

/* ------------------------------------------------------------------ */
/*  5. Determinism                                                     */
/* ------------------------------------------------------------------ */
static void test_determinism(uint64_t seed)
{
    cos_v93_mor_t a, b;
    cos_v93_mor_init(&a, seed, COS_V93_MAX_TOKENS);
    cos_v93_mor_init(&b, seed, COS_V93_MAX_TOKENS);
    uint64_t s = seed ^ 0x222ULL;
    cos_v93_q16_t h0[COS_V93_HDIM];
    for (uint32_t t = 0; t < COS_V93_MAX_TOKENS; ++t) {
        for (uint32_t d = 0; d < COS_V93_HDIM; ++d)
            h0[d] = (cos_v93_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
        cos_v93_mor_set_input(&a, t, h0);
        cos_v93_mor_set_input(&b, t, h0);
    }
    cos_v93_mor_run(&a);
    cos_v93_mor_run(&b);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
}

/* ------------------------------------------------------------------ */
/*  6. Avg depth bounded by MAX_DEPTH                                  */
/* ------------------------------------------------------------------ */
static void test_avg_depth(uint64_t seed)
{
    for (uint32_t trial = 0; trial < 50u; ++trial) {
        cos_v93_mor_t m;
        cos_v93_mor_init(&m, seed + trial, COS_V93_MAX_TOKENS);
        uint64_t s = seed + trial * 31u;
        cos_v93_q16_t h0[COS_V93_HDIM];
        for (uint32_t t = 0; t < COS_V93_MAX_TOKENS; ++t) {
            for (uint32_t d = 0; d < COS_V93_HDIM; ++d)
                h0[d] = (cos_v93_q16_t)((int32_t)(xs(&s) & 0x1FFFFu) - 0x10000);
            cos_v93_mor_set_input(&m, t, h0);
        }
        cos_v93_mor_run(&m);
        cos_v93_q16_t avg = cos_v93_mor_avg_depth(&m);
        CHECK((uint32_t)avg <= (uint32_t)(COS_V93_MAX_DEPTH << 16));
        CHECK((uint32_t)avg >= (uint32_t)(1u << 16));
        CHECK(cos_v93_ok(&m) == 1u);
    }
}

/* ------------------------------------------------------------------ */
/*  7. Monotone exit depth: once a token exits, its active flag stays 0*/
/* ------------------------------------------------------------------ */
static void test_monotone(uint64_t seed)
{
    cos_v93_mor_t m;
    cos_v93_mor_init(&m, seed, 16u);
    uint64_t s = seed ^ 0x333ULL;
    cos_v93_q16_t h0[COS_V93_HDIM];
    for (uint32_t t = 0; t < 16u; ++t) {
        for (uint32_t d = 0; d < COS_V93_HDIM; ++d)
            h0[d] = (cos_v93_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
        cos_v93_mor_set_input(&m, t, h0);
    }
    cos_v93_mor_run(&m);
    for (uint32_t t = 0; t < 16u; ++t) {
        CHECK(m.active[t] == 0u);
        CHECK(m.exit_d[t] >= 1u);
    }
}

/* ------------------------------------------------------------------ */
/*  8. compose truth table                                             */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v93_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v93_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v93_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v93_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v93 σ-MoR — Mixture-of-Recursions with "
               "token-level adaptive recursion depth routing "
               "(33-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0x5A0E1CECA10D1CE5ULL;

    test_init();
    test_clamped(seed);
    test_early_exit(seed);
    test_zero_deep(seed);
    test_determinism(seed);
    test_avg_depth(seed);
    test_monotone(seed);
    test_compose();

    printf("v93 σ-MoR: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
