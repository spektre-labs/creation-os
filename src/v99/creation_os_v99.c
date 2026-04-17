/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v99 — σ-Causal self-test driver.
 */

#include "causal.h"

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
    cos_v99_scm_t a, b;
    cos_v99_scm_init(&a);
    cos_v99_scm_init(&b);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V99_SENTINEL);
    CHECK(cos_v99_ok(&a) == 1u);
    /* Upper triangle must be zero. */
    for (uint32_t i = 0u; i < COS_V99_N; ++i)
        for (uint32_t j = i; j < COS_V99_N; ++j)
            CHECK(a.W[i][j] == 0);
}

/* Simple chain: X0 → X1 → X2, weights (2, 3).
 *   X2 = 3·(2·U0 + U1) + U2  =  6·U0 + 3·U1 + U2                     */
static void test_chain(void)
{
    cos_v99_scm_t m;
    cos_v99_scm_init(&m);
    cos_v99_set_edge(&m, 0, 1, 2 * COS_V99_Q1);
    cos_v99_set_edge(&m, 1, 2, 3 * COS_V99_Q1);

    cos_v99_q16_t U[COS_V99_N] = {0};
    U[0] = 1 * COS_V99_Q1;
    U[1] = 0;
    U[2] = 0;
    cos_v99_observe(&m, U);
    CHECK(m.X[0] == 1 * COS_V99_Q1);
    CHECK(m.X[1] == 2 * COS_V99_Q1);
    CHECK(m.X[2] == 6 * COS_V99_Q1);
}

/* Intervention severs incoming edges: do(X_1 = 0) must force X_1 to 0
 * regardless of U_0 and W_{1,0}, which in turn drives X_2 only through
 * its own noise and direct parent contributions. */
static void test_intervention_severs(void)
{
    cos_v99_scm_t m;
    cos_v99_scm_init(&m);
    cos_v99_set_edge(&m, 0, 1, 2 * COS_V99_Q1);
    cos_v99_set_edge(&m, 1, 2, 3 * COS_V99_Q1);

    cos_v99_q16_t U[COS_V99_N] = {0};
    U[0] = 1 * COS_V99_Q1;
    U[1] = 0;
    U[2] = 0;
    cos_v99_observe(&m, U);
    CHECK(m.X[1] == 2 * COS_V99_Q1);

    cos_v99_do(&m, 1, 0);
    CHECK(m.X[1] == 0);
    CHECK(m.X[2] == 0);
    cos_v99_undo_do(&m, 1);
    CHECK(m.X[1] == 2 * COS_V99_Q1);
    CHECK(m.X[2] == 6 * COS_V99_Q1);
}

/* Counterfactual with unchanged intervention = factual outcome. */
static void test_counterfactual_identity(void)
{
    cos_v99_scm_t m;
    cos_v99_scm_init(&m);
    cos_v99_set_edge(&m, 0, 1, 2 * COS_V99_Q1);
    cos_v99_set_edge(&m, 1, 2, 3 * COS_V99_Q1);

    cos_v99_q16_t U[COS_V99_N] = {0};
    U[0] = 1 * COS_V99_Q1;
    cos_v99_observe(&m, U);
    cos_v99_q16_t factual_x2 = m.X[2];
    /* Counterfactual with do(X_1 = X_1_factual) must reproduce X_2. */
    cos_v99_q16_t cf = cos_v99_counterfactual(&m, U, 1, m.X[1], 2);
    CHECK(cf == factual_x2);
}

/* ATE on a pure chain is the product of path coefficients. */
static void test_ate_path(void)
{
    cos_v99_scm_t m;
    cos_v99_scm_init(&m);
    cos_v99_set_edge(&m, 0, 1, 2 * COS_V99_Q1);
    cos_v99_set_edge(&m, 1, 2, 3 * COS_V99_Q1);

    cos_v99_q16_t U[COS_V99_N] = {0};
    /* ATE_{0 → 2} with hi = 1, lo = 0.   Should equal 6.0 Q16.16. */
    cos_v99_q16_t ate = cos_v99_ate(&m, 0, 1 * COS_V99_Q1, 0, 2, U);
    CHECK(ate == 6 * COS_V99_Q1);

    /* ATE_{1 → 2} with hi = 2, lo = 0.   Should equal 6.0 Q16.16. */
    ate = cos_v99_ate(&m, 1, 2 * COS_V99_Q1, 0, 2, U);
    CHECK(ate == 6 * COS_V99_Q1);
}

/* Back-door check on the classic confounder graph X0 → X1, X0 → X2,
 * X1 → X2.  For cause=1, effect=2, valid Z = {0}. */
static void test_backdoor(void)
{
    cos_v99_scm_t m;
    cos_v99_scm_init(&m);
    cos_v99_set_edge(&m, 0, 1, COS_V99_Q1);       /* confounder → cause */
    cos_v99_set_edge(&m, 0, 2, COS_V99_Q1);       /* confounder → effect */
    cos_v99_set_edge(&m, 1, 2, COS_V99_Q1);       /* cause → effect */

    /* Z = {0} (adjust for the confounder). */
    CHECK(cos_v99_backdoor_valid(&m, 1, 2, 0x1u) == 1u);
    /* Z = {} (no adjustment): invalid. */
    CHECK(cos_v99_backdoor_valid(&m, 1, 2, 0x0u) == 0u);
    /* Z = {2} (effect itself): invalid. */
    CHECK(cos_v99_backdoor_valid(&m, 1, 2, 0x4u) == 0u);
    /* Z contains X_1 descendant X_2: invalid. */
    CHECK(cos_v99_backdoor_valid(&m, 1, 2, 0x5u) == 0u);
}

/* Randomised stress: pick a random DAG with positive weights, verify
 * ATE_{0→N-1}(hi, lo) = (hi-lo) · Σ_paths Π edge weights.               */
static void test_ate_soak(void)
{
    for (uint32_t trial = 0u; trial < 128u; ++trial) {
        cos_v99_scm_t m;
        cos_v99_scm_init(&m);
        uint64_t r = 0xABCDEFu ^ (trial * 2654435761u);

        /* Pure chain: 0→1→…→N-1, weights ∈ [-1, 1] in Q16.16, quantized. */
        int64_t prod = COS_V99_Q1;                /* path product in Q16.16 */
        for (uint32_t i = 1u; i < COS_V99_N; ++i) {
            int32_t w = (int32_t)((int64_t)(xs(&r) & 0x1FFFFu) - 0x10000);
            cos_v99_set_edge(&m, i - 1u, i, (cos_v99_q16_t)w);
            prod = (prod * (int64_t)w) >> 16;
        }
        cos_v99_q16_t U[COS_V99_N] = {0};
        cos_v99_q16_t ate = cos_v99_ate(&m, 0, 2 * COS_V99_Q1, 0, COS_V99_N - 1u, U);
        /* Expect ate ≈ 2 · prod (the (hi - lo) factor). */
        int64_t expected = 2 * prod;
        int32_t diff = iabs((int32_t)ate - (int32_t)expected);
        CHECK(diff <= (int32_t)(COS_V99_N * 4));        /* ≤ 4 ulp per hop */
    }
}

/* Counterfactual linearity: CF(cause, hi) - CF(cause, lo) ≈ ATE(cause, hi, lo). */
static void test_counterfactual_linearity(void)
{
    cos_v99_scm_t m;
    cos_v99_scm_init(&m);
    cos_v99_set_edge(&m, 0, 1, 2 * COS_V99_Q1);
    cos_v99_set_edge(&m, 1, 2, 3 * COS_V99_Q1);
    cos_v99_set_edge(&m, 0, 2, 1 * COS_V99_Q1);   /* direct + chain  */

    for (uint32_t t = 0u; t < 128u; ++t) {
        uint64_t r = 0xDEu + t * 131u;
        cos_v99_q16_t U[COS_V99_N];
        for (uint32_t i = 0u; i < COS_V99_N; ++i)
            U[i] = (cos_v99_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);

        cos_v99_q16_t hi = 1 * COS_V99_Q1;
        cos_v99_q16_t lo = 0;
        cos_v99_q16_t cf_hi = cos_v99_counterfactual(&m, U, 0, hi, 2);
        cos_v99_q16_t cf_lo = cos_v99_counterfactual(&m, U, 0, lo, 2);
        cos_v99_q16_t ate   = cos_v99_ate(&m, 0, hi, lo, 2, U);
        int32_t diff_cf = iabs((int32_t)(cf_hi - cf_lo) - (int32_t)ate);
        CHECK(diff_cf <= 4);
    }
}

/* Determinism. */
static void test_determinism(void)
{
    for (uint32_t trial = 0u; trial < 128u; ++trial) {
        cos_v99_scm_t a, b;
        cos_v99_scm_init(&a);
        cos_v99_scm_init(&b);
        cos_v99_set_edge(&a, 0, 1, (cos_v99_q16_t)(trial * 37u));
        cos_v99_set_edge(&b, 0, 1, (cos_v99_q16_t)(trial * 37u));
        cos_v99_set_edge(&a, 1, 2, (cos_v99_q16_t)(trial * 53u));
        cos_v99_set_edge(&b, 1, 2, (cos_v99_q16_t)(trial * 53u));
        cos_v99_q16_t U[COS_V99_N] = {0};
        U[0] = (cos_v99_q16_t)(trial * 97u);
        cos_v99_observe(&a, U);
        cos_v99_observe(&b, U);
        CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    }
}

static void test_compose(void)
{
    CHECK(cos_v99_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v99_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v99_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v99_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v99 σ-Causal — structural causal model + "
               "do-calculus + back-door adjustment + counterfactual twin "
               "graph (39-bit composed decision)\n");
        return 0;
    }

    test_init();
    test_chain();
    test_intervention_severs();
    test_counterfactual_identity();
    test_ate_path();
    test_backdoor();
    test_ate_soak();
    test_counterfactual_linearity();
    test_determinism();
    test_compose();

    printf("v99 σ-Causal: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
