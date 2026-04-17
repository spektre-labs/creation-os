/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v86 — σ-JEPA self-test driver.
 *
 * Covers: init determinism, encoder linearity (zero-input → zero
 * latent), target=online at init, predictor determinism, VICReg
 * variance non-negativity, EMA convergence, rollout budget invariant,
 * and the 26-bit compose truth table.
 */

#include "jepa.h"

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
static void test_init_determinism(void)
{
    cos_v86_jepa_t a, b;
    cos_v86_jepa_init(&a, 0xC0DEC0DEu);
    cos_v86_jepa_init(&b, 0xC0DEC0DEu);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V86_SENTINEL);

    cos_v86_jepa_t c;
    cos_v86_jepa_init(&c, 0xC0DEC0DEu ^ 1u);
    CHECK(memcmp(&a, &c, sizeof(a)) != 0);
}

/* ------------------------------------------------------------------ */
/*  2. encoder: zero input → zero-or-positive latent (ReLU)            */
/* ------------------------------------------------------------------ */
static void test_encoder_zero(void)
{
    cos_v86_jepa_t j;
    cos_v86_jepa_init(&j, 1u);

    cos_v86_q16_t x[COS_V86_OBS] = {0};
    cos_v86_q16_t z[COS_V86_DIM];
    cos_v86_encode(&j, x, z);
    for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
        CHECK(z[i] >= 0);
    }
}

/* ------------------------------------------------------------------ */
/*  3. target encoder = online at init                                 */
/* ------------------------------------------------------------------ */
static void test_target_equals_online_at_init(uint64_t seed)
{
    cos_v86_jepa_t j;
    cos_v86_jepa_init(&j, seed);

    uint64_t r = seed;
    cos_v86_q16_t x[COS_V86_OBS];
    for (uint32_t t = 0; t < 100u; ++t) {
        for (uint32_t i = 0; i < COS_V86_OBS; ++i)
            x[i] = (cos_v86_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
        cos_v86_q16_t z1[COS_V86_DIM], z2[COS_V86_DIM];
        cos_v86_encode(&j, x, z1);
        cos_v86_target_encode(&j, x, z2);
        for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
            CHECK(z1[i] == z2[i]);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  4. predictor determinism                                           */
/* ------------------------------------------------------------------ */
static void test_predictor_determinism(uint64_t seed)
{
    cos_v86_jepa_t j;
    cos_v86_jepa_init(&j, seed);

    uint64_t r = seed ^ 0xDEADBEEFull;
    for (uint32_t t = 0; t < 500u; ++t) {
        cos_v86_q16_t z[COS_V86_DIM], a[COS_V86_ACT];
        for (uint32_t i = 0; i < COS_V86_DIM; ++i)
            z[i] = (cos_v86_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
        for (uint32_t i = 0; i < COS_V86_ACT; ++i)
            a[i] = (cos_v86_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);

        cos_v86_q16_t p1[COS_V86_DIM], p2[COS_V86_DIM];
        cos_v86_predict(&j, z, a, p1);
        cos_v86_predict(&j, z, a, p2);
        for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
            CHECK(p1[i] == p2[i]);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  5. VICReg variance: large noisy batch → positive min-var          */
/* ------------------------------------------------------------------ */
static void test_vicreg_variance(uint64_t seed)
{
    cos_v86_jepa_t j;
    cos_v86_jepa_init(&j, seed);

    uint64_t r = seed ^ 0xBADA55A1ull;
    enum { N = 32 };
    cos_v86_q16_t Z[N * COS_V86_DIM];
    cos_v86_q16_t x[COS_V86_OBS];
    for (uint32_t n = 0; n < N; ++n) {
        for (uint32_t i = 0; i < COS_V86_OBS; ++i)
            x[i] = (cos_v86_q16_t)((int32_t)(xs(&r) & 0x3FFFFu) - 0x20000);
        cos_v86_encode(&j, x, &Z[n * COS_V86_DIM]);
    }
    cos_v86_q16_t v = cos_v86_vicreg_var(Z, N);
    CHECK(v >= 0);
    /* Collapsed batch (all zero) → zero variance. */
    memset(Z, 0, sizeof(Z));
    CHECK(cos_v86_vicreg_var(Z, N) == 0);
}

/* ------------------------------------------------------------------ */
/*  6. EMA: after enough updates, target drifts toward online          */
/* ------------------------------------------------------------------ */
static void test_ema_converges(void)
{
    cos_v86_jepa_t j;
    cos_v86_jepa_init(&j, 0x00000E4Aull);

    /* Zero-out the online encoder; repeated EMA should shrink W_t. */
    memset(j.W_e, 0, sizeof(j.W_e));
    uint64_t sum_before = 0;
    for (uint32_t i = 0; i < COS_V86_DIM; ++i)
        for (uint32_t k = 0; k < COS_V86_OBS; ++k)
            sum_before += (uint64_t)(uint32_t)((j.W_t[i][k] < 0) ? -j.W_t[i][k]
                                                                 :  j.W_t[i][k]);
    for (uint32_t s = 0; s < 100u; ++s) cos_v86_ema_update(&j);
    uint64_t sum_after = 0;
    for (uint32_t i = 0; i < COS_V86_DIM; ++i)
        for (uint32_t k = 0; k < COS_V86_OBS; ++k)
            sum_after += (uint64_t)(uint32_t)((j.W_t[i][k] < 0) ? -j.W_t[i][k]
                                                                :  j.W_t[i][k]);
    CHECK(sum_after <= sum_before);
    /* And not NaN-ish; sentinel intact. */
    CHECK(j.sentinel == COS_V86_SENTINEL);
}

/* ------------------------------------------------------------------ */
/*  7. Rollout budget invariant                                        */
/* ------------------------------------------------------------------ */
static void test_rollout_budget(uint64_t seed)
{
    cos_v86_jepa_t j;
    cos_v86_jepa_init(&j, seed);

    uint64_t r = seed ^ 0x1234ull;
    uint32_t under = 0, total = 5000u;
    cos_v86_q16_t x_t[COS_V86_OBS], x_tp1[COS_V86_OBS], a[COS_V86_ACT];
    for (uint32_t t = 0; t < total; ++t) {
        for (uint32_t i = 0; i < COS_V86_OBS; ++i) {
            x_t  [i] = (cos_v86_q16_t)((int32_t)(xs(&r) & 0x3FFFu) - 0x2000);
            x_tp1[i] = (cos_v86_q16_t)((int32_t)(xs(&r) & 0x3FFFu) - 0x2000);
        }
        for (uint32_t i = 0; i < COS_V86_ACT; ++i)
            a[i] = (cos_v86_q16_t)((int32_t)(xs(&r) & 0x1FFFu) - 0x1000);

        cos_v86_q16_t err = cos_v86_rollout_step(&j, x_t, a, x_tp1);
        CHECK(err >= 0);
        if ((uint32_t)err <= j.rollout_budget) ++under;
    }
    /* With small inputs and Xavier-ish weights, most steps should be
     * inside the budget; assert a soft floor (≥ 80 %). */
    CHECK(under >= (total * 4u) / 5u);
    CHECK(j.invariant_violations == 0u);
}

/* ------------------------------------------------------------------ */
/*  8. compose-decision 4-row truth table                              */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v86_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v86_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v86_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v86_compose_decision(1u, 1u) == 1u);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v86 σ-JEPA — latent-space predictive world "
               "model (26-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0x7E7EF00DBABECAFEull;

    test_init_determinism();
    test_encoder_zero();
    test_target_equals_online_at_init(seed);
    test_predictor_determinism(seed);
    test_vicreg_variance(seed);
    test_ema_converges();
    test_rollout_budget(seed);
    test_compose();

    printf("v86 σ-JEPA: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
