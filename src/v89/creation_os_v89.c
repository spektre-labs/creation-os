/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v89 — σ-Spiking self-test driver.
 *
 * Covers: LIF decay monotonicity, threshold fire, graded payload,
 * synapse propagation, STDP causal potentiation, STDP acausal
 * depression, weight clamp invariant, and the 29-bit compose truth
 * table.
 */

#include "spiking.h"

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

/* ------------------------------------------------------------------ */
/*  1. LIF decay: |V| monotonically shrinks for positive and negative  */
/* ------------------------------------------------------------------ */
static void test_lif_decay(void)
{
    for (int32_t V0 = 1; V0 < (1 << 18); V0 += 137) {
        cos_v89_q16_t V = V0;
        cos_v89_q16_t prev = V;
        for (uint32_t t = 0; t < 64u; ++t) {
            V = cos_v89_lif_decay(V, 4u);
            CHECK(V <= prev);
            CHECK(V >= 0);
            prev = V;
        }
    }
    for (int32_t V0 = -1; V0 > -(1 << 18); V0 -= 137) {
        cos_v89_q16_t V = V0;
        cos_v89_q16_t prev = V;
        for (uint32_t t = 0; t < 64u; ++t) {
            V = cos_v89_lif_decay(V, 4u);
            CHECK(V >= prev);      /* less negative, i.e. larger */
            CHECK(V <= 0);
            prev = V;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  2. Fire with graded payload                                        */
/* ------------------------------------------------------------------ */
static void test_fire_graded(void)
{
    cos_v89_net_t n;
    cos_v89_net_init(&n);
    cos_v89_inject(&n, 0, (int32_t)(3 << 16));       /* +3.0 */
    CHECK(n.V[0] == (int32_t)(3 << 16));
    uint32_t fired = cos_v89_step(&n);
    CHECK(fired == 1u);
    CHECK(n.V[0] == 0);
    CHECK(n.spike_count == 1u);
    CHECK(n.last_spike_t[0] == 0);
    /* Graded payload should be positive (V_pre_leak - V_theta ≈ +2). */
    CHECK(n.last_spike_payload[0] > 0);
}

/* ------------------------------------------------------------------ */
/*  3. Propagation: pre fires, post gains potential                    */
/* ------------------------------------------------------------------ */
static void test_propagation(void)
{
    cos_v89_net_t n;
    cos_v89_net_init(&n);
    uint32_t s = cos_v89_connect(&n, 0, 1, (int32_t)(1 << 15));  /* w = 0.5 */
    CHECK(s == 0u);

    cos_v89_inject(&n, 0, (int32_t)(3 << 16));
    CHECK(n.V[1] == 0);
    cos_v89_step(&n);
    /* Post should have gained ~0.5 * (payload≈2). */
    CHECK(n.V[1] > 0);
}

/* ------------------------------------------------------------------ */
/*  4. STDP causal: post fires just after pre -> weight goes up        */
/* ------------------------------------------------------------------ */
static void test_stdp_causal(void)
{
    cos_v89_net_t n;
    cos_v89_net_init(&n);
    uint32_t s = cos_v89_connect(&n, 0, 1, 0);
    int32_t w0 = cos_v89_weight(&n, s);

    /* Tick 0: pre fires. */
    cos_v89_inject(&n, 0, (int32_t)(3 << 16));
    cos_v89_step(&n);

    /* Tick 1: post fires via an external injection (bypassing the
     * still-zero synaptic weight). */
    cos_v89_inject(&n, 1, (int32_t)(3 << 16));
    cos_v89_step(&n);

    int32_t w1 = cos_v89_weight(&n, s);
    CHECK(w1 > w0);      /* causal potentiation */
}

/* ------------------------------------------------------------------ */
/*  5. STDP acausal: post before pre -> weight goes down               */
/* ------------------------------------------------------------------ */
static void test_stdp_acausal(void)
{
    cos_v89_net_t n;
    cos_v89_net_init(&n);
    uint32_t s = cos_v89_connect(&n, 0, 1, 0);
    int32_t w0 = cos_v89_weight(&n, s);

    /* Tick 0: post fires. */
    cos_v89_inject(&n, 1, (int32_t)(3 << 16));
    cos_v89_step(&n);

    /* Tick 1: pre fires. */
    cos_v89_inject(&n, 0, (int32_t)(3 << 16));
    cos_v89_step(&n);

    int32_t w1 = cos_v89_weight(&n, s);
    CHECK(w1 < w0);
}

/* ------------------------------------------------------------------ */
/*  6. Weight clamp: STDP cannot push weight outside [-w_max, w_max]   */
/* ------------------------------------------------------------------ */
static void test_weight_clamp(void)
{
    cos_v89_net_t n;
    cos_v89_net_init(&n);
    /* Pre-load synapse at +w_max; repeated causal STDP must not pump it
     * past w_max. */
    uint32_t s = cos_v89_connect(&n, 0, 1, n.w_max);
    for (uint32_t t = 0; t < 200u; ++t) {
        cos_v89_inject(&n, 0, (int32_t)(3 << 16));
        cos_v89_step(&n);
        cos_v89_inject(&n, 1, (int32_t)(3 << 16));
        cos_v89_step(&n);
    }
    CHECK(cos_v89_weight(&n, s) <= n.w_max);
    CHECK(cos_v89_weight(&n, s) >= -n.w_max);
    CHECK(cos_v89_ok(&n) == 1u);
}

/* ------------------------------------------------------------------ */
/*  7. Invariant: no neuron's |V| exceeds V_max                        */
/* ------------------------------------------------------------------ */
static void test_overflow_invariant(void)
{
    cos_v89_net_t n;
    cos_v89_net_init(&n);
    /* Hammer neuron 0 with massive injections; V clamps at V_max. */
    for (uint32_t t = 0; t < 1000u; ++t) {
        cos_v89_inject(&n, 0, (int32_t)(1 << 30));
        cos_v89_step(&n);
        CHECK(n.V[0] <= n.V_max && n.V[0] >= -n.V_max);
    }
    CHECK(cos_v89_ok(&n) == 1u);
}

/* ------------------------------------------------------------------ */
/*  8. compose truth table                                             */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v89_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v89_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v89_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v89_compose_decision(1u, 1u) == 1u);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v89 σ-Spiking — Loihi-3 style graded-spike "
               "LIF + STDP neuromorphic (29-bit composed decision)\n");
        return 0;
    }

    test_lif_decay();
    test_fire_graded();
    test_propagation();
    test_stdp_causal();
    test_stdp_acausal();
    test_weight_clamp();
    test_overflow_invariant();
    test_compose();

    printf("v89 σ-Spiking: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
