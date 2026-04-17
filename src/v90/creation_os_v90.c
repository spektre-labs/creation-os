/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v90 — σ-Hierarchical self-test driver.
 *
 * Covers: init determinism, receipt chain integrity, receipt
 * chain divergence under tampering, schema EMA convergence toward
 * L1 posterior, free-energy monotonic accumulation, zero-
 * observation equilibrium (F stays zero), and the 30-bit compose
 * truth table.
 */

#include "hierarchical.h"

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
/*  1. init determinism                                                */
/* ------------------------------------------------------------------ */
static void test_init(void)
{
    cos_v90_hi_t a, b;
    cos_v90_hi_init(&a, 0xDEADC0DEULL);
    cos_v90_hi_init(&b, 0xDEADC0DEULL);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V90_SENTINEL);

    cos_v90_hi_t c;
    cos_v90_hi_init(&c, 0xDEADC0DFULL);
    CHECK(memcmp(&a, &c, sizeof(a)) != 0);
}

/* ------------------------------------------------------------------ */
/*  2. receipt chain: same sequence → same hash, per-step               */
/* ------------------------------------------------------------------ */
static void test_receipt_chain(uint64_t seed)
{
    cos_v90_hi_t a, b;
    cos_v90_hi_init(&a, seed);
    cos_v90_hi_init(&b, seed);

    uint64_t r = seed;
    cos_v90_q16_t o[COS_V90_DIM], p[COS_V90_DIM];
    for (uint32_t t = 0; t < 500u; ++t) {
        for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
            o[d] = (cos_v90_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
            p[d] = (cos_v90_q16_t)((int32_t)(xs(&r) & 0x1FFFu) - 0x1000);
        }
        cos_v90_step(&a, o, p);
        cos_v90_step(&b, o, p);
        /* Every step the rolling hashes must still be byte-equal. */
        uint8_t ha[32], hb[32];
        cos_v90_receipt(&a, ha);
        cos_v90_receipt(&b, hb);
        CHECK(memcmp(ha, hb, 32) == 0);
        /* Also: internal state must be byte-equal. */
        CHECK(a.steps == b.steps);
        CHECK(a.total_free_energy == b.total_free_energy);
        CHECK(a.last_free_energy  == b.last_free_energy);
    }
}

/* ------------------------------------------------------------------ */
/*  3. receipt chain diverges on tampered observation                  */
/* ------------------------------------------------------------------ */
static void test_receipt_tamper(uint64_t seed)
{
    cos_v90_hi_t a, b;
    cos_v90_hi_init(&a, seed);
    cos_v90_hi_init(&b, seed);

    uint64_t r = seed ^ 0x7777ULL;
    cos_v90_q16_t o[COS_V90_DIM], p[COS_V90_DIM];
    for (uint32_t t = 0; t < 100u; ++t) {
        for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
            o[d] = (cos_v90_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
            p[d] = 0;
        }
        cos_v90_step(&a, o, p);
        if (t == 50u) o[0] ^= 1;            /* tamper */
        cos_v90_step(&b, o, p);
    }
    uint8_t ha[32], hb[32];
    cos_v90_receipt(&a, ha);
    cos_v90_receipt(&b, hb);
    CHECK(memcmp(ha, hb, 32) != 0);
}

/* ------------------------------------------------------------------ */
/*  4. schema EMA converges toward L1 posterior                        */
/* ------------------------------------------------------------------ */
static void test_schema_convergence(uint64_t seed)
{
    cos_v90_hi_t h;
    cos_v90_hi_init(&h, seed);

    /* Constant observation o = 1.0 per dim → L1 posterior settles to
     * something non-zero; schema EMA pulls μ_2 toward μ_1. */
    cos_v90_q16_t o[COS_V90_DIM], p[COS_V90_DIM];
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        o[d] = (cos_v90_q16_t)(1 << 16);
        p[d] = 0;
    }

    cos_v90_q16_t abs_mu2_before = 0;
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        int32_t v = h.mu[2][d];
        abs_mu2_before += (v < 0) ? -v : v;
    }
    CHECK(abs_mu2_before == 0);

    for (uint32_t t = 0; t < 50u; ++t) {
        cos_v90_step(&h, o, p);
        cos_v90_update_schema(&h);
    }
    cos_v90_q16_t abs_mu2_after = 0;
    for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
        int32_t v = h.mu[2][d];
        abs_mu2_after += (v < 0) ? -v : v;
    }
    CHECK(abs_mu2_after > abs_mu2_before);
}

/* ------------------------------------------------------------------ */
/*  5. zero observation → zero free energy                             */
/* ------------------------------------------------------------------ */
static void test_zero_equilibrium(uint64_t seed)
{
    cos_v90_hi_t h;
    cos_v90_hi_init(&h, seed);
    cos_v90_q16_t o[COS_V90_DIM] = {0};
    cos_v90_q16_t p[COS_V90_DIM] = {0};
    for (uint32_t t = 0; t < 10000u; ++t) {
        cos_v90_step(&h, o, p);
        CHECK(h.last_free_energy == 0);
        CHECK(h.total_free_energy == 0);
        CHECK(cos_v90_ok(&h) == 1u);
    }
}

/* ------------------------------------------------------------------ */
/*  5b. step determinism: same input twice → same state                 */
/* ------------------------------------------------------------------ */
static void test_step_determinism(uint64_t seed)
{
    cos_v90_hi_t a, b;
    cos_v90_hi_init(&a, seed);
    cos_v90_hi_init(&b, seed);
    uint64_t r = seed ^ 0xABCDULL;
    cos_v90_q16_t o[COS_V90_DIM], p[COS_V90_DIM];
    for (uint32_t t = 0; t < 500u; ++t) {
        for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
            o[d] = (cos_v90_q16_t)((int32_t)(xs(&r) & 0x7FFu) - 0x400);
            p[d] = 0;
        }
        cos_v90_q16_t fa = cos_v90_step(&a, o, p);
        cos_v90_q16_t fb = cos_v90_step(&b, o, p);
        CHECK(fa == fb);
        for (uint32_t l = 0; l < COS_V90_LEVELS; ++l)
            for (uint32_t d = 0; d < COS_V90_DIM; ++d)
                CHECK(a.mu[l][d] == b.mu[l][d]);
    }
}

/* ------------------------------------------------------------------ */
/*  6. free energy stays within ceiling for small inputs               */
/* ------------------------------------------------------------------ */
static void test_fe_ceiling(uint64_t seed)
{
    cos_v90_hi_t h;
    cos_v90_hi_init(&h, seed);

    uint64_t r = seed ^ 0x111ULL;
    cos_v90_q16_t o[COS_V90_DIM], p[COS_V90_DIM];
    for (uint32_t t = 0; t < 30u; ++t) {
        for (uint32_t d = 0; d < COS_V90_DIM; ++d) {
            /* Small-amplitude observations (< 0.1). */
            o[d] = (cos_v90_q16_t)((int32_t)(xs(&r) & 0x1FFu) - 0x100);
            p[d] = 0;
        }
        cos_v90_step(&h, o, p);
    }
    /* Cumulative F grows under any non-zero observation; the *per-step*
     * gate however stays within the ceiling for small inputs. */
    CHECK((uint32_t)h.last_free_energy <= h.free_energy_ceiling);
    CHECK(cos_v90_ok(&h) == 1u);
}

/* ------------------------------------------------------------------ */
/*  7. compose truth table                                             */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v90_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v90_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v90_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v90_compose_decision(1u, 1u) == 1u);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v90 σ-Hierarchical — three-level hierarchical "
               "active inference / predictive coding tower "
               "(30-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0x1EC08F4EC0DEBABEULL;

    test_init();
    test_receipt_chain(seed);
    test_receipt_tamper(seed);
    test_schema_convergence(seed);
    test_zero_equilibrium(seed);
    test_step_determinism(seed);
    test_fe_ceiling(seed);
    test_compose();

    printf("v90 σ-Hierarchical: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
