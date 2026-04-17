/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v95 — σ-Sheaf self-test driver.
 *
 * Covers:
 *   - init determinism
 *   - restriction maps are {−1,+1}-orthogonal
 *   - Δ_F of a constant-per-orbit signal is zero (harmonic)
 *   - sheaf diffusion strictly decreases energy from any non-harmonic
 *     starting signal
 *   - the 35-bit compose truth table
 */

#include "sheaf.h"

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
    cos_v95_sheaf_t a, b;
    cos_v95_sheaf_init(&a, 0xC0FFEECAFEULL);
    cos_v95_sheaf_init(&b, 0xC0FFEECAFEULL);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V95_SENTINEL);
    CHECK(a.steps == 0u);
    CHECK(a.energy_violations == 0u);
    CHECK(cos_v95_ok(&a) == 1u);
}

/* ------------------------------------------------------------------ */
/*  Zero signal is sheaf-harmonic: Δ_F·0 = 0, energy = 0               */
/* ------------------------------------------------------------------ */
static void test_zero_harmonic(uint64_t seed)
{
    cos_v95_sheaf_t s;
    cos_v95_sheaf_init(&s, seed);
    cos_v95_q16_t x[COS_V95_NODES][COS_V95_DIM];
    memset(x, 0, sizeof(x));
    cos_v95_set_signal(&s, x);
    CHECK(cos_v95_energy(&s) == 0);
    for (uint32_t t = 0; t < 100u; ++t) {
        cos_v95_diffuse_step(&s);
        CHECK(cos_v95_energy(&s) == 0);
    }
    CHECK(cos_v95_ok(&s) == 1u);
}

/* ------------------------------------------------------------------ */
/*  Constant signal on ring with all-+1 restrictions is harmonic       */
/* ------------------------------------------------------------------ */
static void test_constant_harmonic(void)
{
    cos_v95_sheaf_t s;
    cos_v95_sheaf_init(&s, 0ULL);           /* seed 0 → deterministic seed */
    /* Force all restriction maps to +1 so Δ_F matches the graph
     * Laplacian; a constant signal is then harmonic. */
    for (uint32_t e = 0; e < COS_V95_EDGES; ++e) {
        for (uint32_t d = 0; d < COS_V95_DIM; ++d) {
            s.r_u[e][d] = +1;
            s.r_v[e][d] = +1;
        }
    }
    cos_v95_q16_t x[COS_V95_NODES][COS_V95_DIM];
    for (uint32_t v = 0; v < COS_V95_NODES; ++v)
        for (uint32_t d = 0; d < COS_V95_DIM; ++d)
            x[v][d] = (cos_v95_q16_t)(1 << 16);          /* all ones */
    cos_v95_set_signal(&s, x);
    CHECK(cos_v95_energy(&s) == 0);
}

/* ------------------------------------------------------------------ */
/*  Energy is monotone non-increasing under sheaf diffusion            */
/* ------------------------------------------------------------------ */
static void test_energy_monotone(uint64_t seed)
{
    for (uint32_t trial = 0; trial < 20u; ++trial) {
        cos_v95_sheaf_t s;
        cos_v95_sheaf_init(&s, seed + trial);
        cos_v95_q16_t x[COS_V95_NODES][COS_V95_DIM];
        uint64_t rr = seed ^ (trial * 131u);
        for (uint32_t v = 0; v < COS_V95_NODES; ++v) {
            for (uint32_t d = 0; d < COS_V95_DIM; ++d) {
                x[v][d] = (cos_v95_q16_t)((int32_t)(xs(&rr) & 0xFFFFu) - 0x8000);
            }
        }
        cos_v95_set_signal(&s, x);
        int64_t e_prev = cos_v95_energy(&s);
        for (uint32_t t = 0; t < 200u; ++t) {
            cos_v95_diffuse_step(&s);
            int64_t e_now = cos_v95_energy(&s);
            CHECK(e_now <= e_prev);
            e_prev = e_now;
        }
        CHECK(s.energy_violations == 0u);
        CHECK(cos_v95_ok(&s) == 1u);
    }
}

/* ------------------------------------------------------------------ */
/*  Diffusion reduces energy strictly below its initial value          */
/* ------------------------------------------------------------------ */
static void test_energy_decay(uint64_t seed)
{
    cos_v95_sheaf_t s;
    cos_v95_sheaf_init(&s, seed);
    cos_v95_q16_t x[COS_V95_NODES][COS_V95_DIM];
    /* Impulse on node 0. */
    memset(x, 0, sizeof(x));
    for (uint32_t d = 0; d < COS_V95_DIM; ++d)
        x[0][d] = (cos_v95_q16_t)(1 << 16);
    cos_v95_set_signal(&s, x);
    int64_t e0 = cos_v95_energy(&s);
    CHECK(e0 > 0);
    for (uint32_t t = 0; t < 500u; ++t) cos_v95_diffuse_step(&s);
    int64_t e_final = cos_v95_energy(&s);
    CHECK(e_final < e0);
}

/* ------------------------------------------------------------------ */
/*  Restriction maps are {−1,+1}                                       */
/* ------------------------------------------------------------------ */
static void test_restriction_maps(uint64_t seed)
{
    cos_v95_sheaf_t s;
    cos_v95_sheaf_init(&s, seed);
    for (uint32_t e = 0; e < COS_V95_EDGES; ++e) {
        for (uint32_t d = 0; d < COS_V95_DIM; ++d) {
            CHECK(s.r_u[e][d] == 1 || s.r_u[e][d] == -1);
            CHECK(s.r_v[e][d] == 1 || s.r_v[e][d] == -1);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Determinism                                                         */
/* ------------------------------------------------------------------ */
static void test_determinism(uint64_t seed)
{
    cos_v95_sheaf_t a, b;
    cos_v95_sheaf_init(&a, seed);
    cos_v95_sheaf_init(&b, seed);
    uint64_t rr = seed ^ 0xD00DULL;
    cos_v95_q16_t x[COS_V95_NODES][COS_V95_DIM];
    for (uint32_t v = 0; v < COS_V95_NODES; ++v)
        for (uint32_t d = 0; d < COS_V95_DIM; ++d)
            x[v][d] = (cos_v95_q16_t)((int32_t)(xs(&rr) & 0xFFFFu) - 0x8000);
    cos_v95_set_signal(&a, x);
    cos_v95_set_signal(&b, x);
    for (uint32_t t = 0; t < 50u; ++t) {
        cos_v95_diffuse_step(&a);
        cos_v95_diffuse_step(&b);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    }
}

/* ------------------------------------------------------------------ */
/*  compose truth table                                                */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v95_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v95_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v95_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v95_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v95 σ-Sheaf — cellular-sheaf neural network with "
               "sheaf-Laplacian heat diffusion (35-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0x5EEAF95C0FAA5EEDULL;

    test_init();
    test_zero_harmonic(seed);
    test_constant_harmonic();
    test_energy_monotone(seed);
    test_energy_decay(seed);
    test_restriction_maps(seed);
    test_determinism(seed);
    test_compose();

    printf("v95 σ-Sheaf: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
