/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v83 — σ-Agentic self-test driver.
 */

#include "agentic.h"
#include "../v81/lattice.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t g_pass = 0;
static uint64_t g_fail = 0;

#define CHECK(cond) do {                                 \
    if ((cond)) { ++g_pass; }                            \
    else {                                               \
        if (g_fail < 16) {                               \
            fprintf(stderr, "FAIL %s:%d: %s\n",          \
                    __FILE__, __LINE__, #cond);          \
        }                                                \
        ++g_fail;                                        \
    }                                                    \
} while (0)

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    *s = x;
    return x;
}

/* ================================================================== */
/*  1. plan_step determinism                                           */
/* ================================================================== */

static void test_plan_step_determinism(uint64_t seed)
{
    uint64_t r = seed;
    for (int trial = 0; trial < 500; ++trial) {
        cos_v83_state_t s1 = { (int32_t)xs(&r), (int32_t)xs(&r) };
        cos_v83_state_t s2 = s1;
        cos_v83_plan_cell_t cell = { (uint32_t)(xs(&r) & 0x1Fu), (int32_t)xs(&r) };
        cos_v83_plan_step(&s1, &cell);
        cos_v83_plan_step(&s2, &cell);
        CHECK(s1.x == s2.x);
        CHECK(s1.v == s2.v);
    }
}

/* ================================================================== */
/*  2. cold-start step accepts (no prior history, expectation = 0)     */
/* ================================================================== */

static void test_cold_start(void)
{
    cos_v83_agent_t a;
    cos_v83_agent_init(&a, 1 << 20);  /* huge budget */
    cos_v83_plan_cell_t plan[4] = {
        {0, 100}, {1, 50}, {2, 0}, {3, 0}
    };
    uint32_t acc = cos_v83_step(&a, plan, 4);
    CHECK(acc == 1u);
    CHECK(a.accepts == 1u);
    CHECK(a.rejects == 0u);
    CHECK(a.count   == 1u);
    CHECK(a.invariant_violations == 0u);
}

/* ================================================================== */
/*  3. rollback on reject restores live state                          */
/* ================================================================== */

static void test_rollback(void)
{
    cos_v83_agent_t a;
    /* Budget large enough for the first small plan but not the wild
     * second plan.  First plan surprise ≈ 1000 → energy +125, so we
     * set the budget to 200 so the first accepts and the second
     * (surprise in the millions) rejects. */
    cos_v83_agent_init(&a, 200);

    /* First step: cold, accepted (small trajectory). */
    cos_v83_plan_cell_t p0[2] = { {0, 1000}, {1, 0} };
    CHECK(cos_v83_step(&a, p0, 2) == 1u);
    cos_v83_state_t after_first = a.live;

    /* Force a wild plan whose trajectory will be unlike the prior,
     * triggering a huge surprise and a reject. */
    cos_v83_plan_cell_t p1[4] = {
        {10, 0}, {11, 0}, {0, 50000000}, {1, -50000000}
    };
    uint32_t acc = cos_v83_step(&a, p1, 4);
    CHECK(acc == 0u);
    CHECK(a.rejects == 1u);
    /* live must equal after_first (rolled back). */
    CHECK(a.live.x == after_first.x);
    CHECK(a.live.v == after_first.v);
}

/* ================================================================== */
/*  4. energy cannot exceed budget after acceptance                    */
/* ================================================================== */

static void test_energy_invariant(uint64_t seed)
{
    uint64_t r = seed;
    for (int trial = 0; trial < 50; ++trial) {
        cos_v83_agent_t a;
        cos_v83_q16_t budget = (cos_v83_q16_t)(1024 + (xs(&r) & 0xFFFF));
        cos_v83_agent_init(&a, budget);
        for (int step = 0; step < 40; ++step) {
            cos_v83_plan_cell_t plan[8];
            for (int i = 0; i < 8; ++i) {
                plan[i].op = (uint32_t)(xs(&r) & 0x1Fu);
                plan[i].param = (int32_t)(xs(&r) & 0xFFFFu);
            }
            cos_v83_step(&a, plan, 8);
            /* After every accepted step, energy must be <= budget. */
            CHECK(a.energy <= budget);
        }
    }
}

/* ================================================================== */
/*  5. receipt chain changes every step                                */
/* ================================================================== */

static void test_receipt_chain(uint64_t seed)
{
    cos_v83_agent_t a;
    cos_v83_agent_init(&a, 1 << 20);
    uint8_t prev[32];
    memcpy(prev, a.receipt, 32);
    uint64_t r = seed;
    for (int i = 0; i < 64; ++i) {
        cos_v83_plan_cell_t plan[2] = {
            {(uint32_t)(xs(&r) & 0x1Fu), (int32_t)xs(&r)},
            {(uint32_t)(xs(&r) & 0x1Fu), (int32_t)xs(&r)}
        };
        cos_v83_step(&a, plan, 2);
        CHECK(memcmp(prev, a.receipt, 32) != 0);
        memcpy(prev, a.receipt, 32);
    }
}

/* ================================================================== */
/*  6. determinism of full trajectory                                  */
/* ================================================================== */

static void test_determinism(uint64_t seed)
{
    cos_v83_agent_t a, b;
    cos_v83_agent_init(&a, 1 << 20);
    cos_v83_agent_init(&b, 1 << 20);

    uint64_t r = seed;
    for (int step = 0; step < 64; ++step) {
        cos_v83_plan_cell_t plan[4];
        for (int i = 0; i < 4; ++i) {
            plan[i].op = (uint32_t)(xs(&r) & 0x1Fu);
            plan[i].param = (int32_t)xs(&r);
        }
        uint32_t aa = cos_v83_step(&a, plan, 4);
        uint32_t bb = cos_v83_step(&b, plan, 4);
        CHECK(aa == bb);
    }
    CHECK(a.live.x == b.live.x);
    CHECK(a.live.v == b.live.v);
    CHECK(a.accepts == b.accepts);
    CHECK(a.rejects == b.rejects);
    CHECK(memcmp(a.receipt, b.receipt, 32) == 0);
}

/* ================================================================== */
/*  7. consolidation reduces memory, preserves budget invariant         */
/* ================================================================== */

static void test_consolidation(uint64_t seed)
{
    cos_v83_agent_t a;
    cos_v83_agent_init(&a, 1 << 20);
    uint64_t r = seed;
    for (int i = 0; i < 16; ++i) {
        cos_v83_plan_cell_t p[2] = {
            {(uint32_t)(xs(&r) & 0x1Fu), (int32_t)xs(&r)},
            {(uint32_t)(xs(&r) & 0x1Fu), (int32_t)xs(&r)}
        };
        cos_v83_step(&a, p, 2);
    }
    CHECK(a.count > 0u);
    uint32_t before = a.count;
    cos_v83_q16_t e_before = a.energy;
    uint32_t after = cos_v83_consolidate(&a, before);
    CHECK(after == 1u);
    CHECK(a.count == 1u);
    CHECK(a.energy <= e_before);
}

/* ================================================================== */
/*  8. compose truth table                                             */
/* ================================================================== */

static void test_compose(void)
{
    for (uint32_t v82 = 0; v82 < 2; ++v82)
        for (uint32_t v83 = 0; v83 < 2; ++v83)
            CHECK(cos_v83_compose_decision(v82, v83) == (v82 & v83));
}

/* ================================================================== */
/*  9. soak: 10k steps                                                  */
/* ================================================================== */

static void soak(uint64_t seed)
{
    cos_v83_agent_t a;
    cos_v83_agent_init(&a, 1 << 18);

    uint64_t r = seed;
    for (int step = 0; step < 10000; ++step) {
        cos_v83_plan_cell_t plan[4];
        for (int i = 0; i < 4; ++i) {
            plan[i].op = (uint32_t)(xs(&r) & 0x1Fu);
            plan[i].param = (int32_t)xs(&r);
        }
        cos_v83_step(&a, plan, 4);
        CHECK(a.sentinel == COS_V83_SENTINEL);
        /* Periodically consolidate. */
        if ((step % 500) == 499) {
            cos_v83_consolidate(&a, a.count);
        }
    }
    CHECK(a.invariant_violations == 0u);
    CHECK(cos_v83_ok(&a) == 1u);
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v83 σ-Agentic — PLAN→ROLL→SURPRISE→ENERGY "
               "causal loop (23-bit composed decision)\n");
        return 0;
    }
    const uint64_t seed = 0xA5E7E8A11C7A5E7EULL;

    test_plan_step_determinism(seed);
    test_cold_start();
    test_rollback();
    test_energy_invariant(seed);
    test_receipt_chain(seed);
    test_determinism(seed);
    test_consolidation(seed);
    test_compose();
    soak(seed);

    printf("v83 σ-Agentic: %" PRIu64 " PASS / %" PRIu64 " FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
