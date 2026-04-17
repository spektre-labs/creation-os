/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v85 — σ-Formal self-test driver.
 *
 * Fuzzes the TLA-style checker with deliberately broken traces and
 * verifies that every class of temporal violation is caught.
 */

#include "formal.h"

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

/* Bit 0 of state: composed_ok.  Bit 1: halt.  Bit 2: accept.
 * Bit 3: memory_roll_back. */
static uint32_t pred_composed_ok(uint64_t s)    { return (uint32_t)(s & 1ULL); }
static uint32_t pred_halt(uint64_t s)           { return (uint32_t)((s >> 1) & 1ULL); }
__attribute__((unused))
static uint32_t pred_accept(uint64_t s)         { return (uint32_t)((s >> 2) & 1ULL); }
static uint32_t pred_reject(uint64_t s)         { return (uint32_t)((((s >> 2) & 1ULL)) ? 0u : 1u); }
static uint32_t pred_rollback(uint64_t s)       { return (uint32_t)((s >> 3) & 1ULL); }

/* ================================================================== */
/*  1. ALWAYS(composed_ok) on an all-1 trace passes                    */
/* ================================================================== */

static void test_always_happy(void)
{
    cos_v85_checker_t c;
    cos_v85_checker_init(&c);
    cos_v85_register_always(&c, pred_composed_ok);

    for (int i = 0; i < 100; ++i) cos_v85_observe(&c, 0x1ULL);
    CHECK((cos_v85_status(&c) & 1u) == 1u);
    CHECK(cos_v85_ok(&c) == 1u);
}

/* ================================================================== */
/*  2. ALWAYS caught by a single violating state                       */
/* ================================================================== */

static void test_always_caught(void)
{
    cos_v85_checker_t c;
    cos_v85_checker_init(&c);
    uint32_t id = cos_v85_register_always(&c, pred_composed_ok);
    for (int i = 0; i < 50; ++i) cos_v85_observe(&c, 0x1ULL);
    cos_v85_observe(&c, 0x0ULL);   /* composed_ok = 0 */
    for (int i = 0; i < 50; ++i) cos_v85_observe(&c, 0x1ULL);
    CHECK((cos_v85_status(&c) & 1u) == 0u);
    CHECK(cos_v85_ok(&c) == 0u);
    CHECK(cos_v85_witness(&c, id) == 0x0ULL);
}

/* ================================================================== */
/*  3. EVENTUALLY not fired → fails                                    */
/* ================================================================== */

static void test_eventually_unfired(void)
{
    cos_v85_checker_t c;
    cos_v85_checker_init(&c);
    cos_v85_register_eventually(&c, pred_halt);

    for (int i = 0; i < 100; ++i) cos_v85_observe(&c, 0x1ULL);  /* halt bit stays 0 */
    CHECK((cos_v85_status(&c) & 2u) == 0u);
}

/* ================================================================== */
/*  4. EVENTUALLY fired once → passes                                  */
/* ================================================================== */

static void test_eventually_fired(void)
{
    cos_v85_checker_t c;
    cos_v85_checker_init(&c);
    cos_v85_register_eventually(&c, pred_halt);

    for (int i = 0; i < 100; ++i) {
        uint64_t st = (i == 50) ? 0x3ULL : 0x1ULL;
        cos_v85_observe(&c, st);
    }
    CHECK((cos_v85_status(&c) & 2u) != 0u);
}

/* ================================================================== */
/*  5. RESPONDS: reject ~> rollback                                    */
/* ================================================================== */

static void test_responds_pair(void)
{
    cos_v85_checker_t c;
    cos_v85_checker_init(&c);
    cos_v85_register_responds(&c, pred_reject, pred_rollback);

    /* Trace: accept, accept, reject, rollback, accept. */
    cos_v85_observe(&c, 0x5ULL);      /* accept */
    cos_v85_observe(&c, 0x5ULL);      /* accept */
    cos_v85_observe(&c, 0x1ULL);      /* reject (accept bit = 0), composed ok */
    cos_v85_observe(&c, 0x9ULL);      /* rollback bit set */
    cos_v85_observe(&c, 0x5ULL);
    CHECK((cos_v85_status(&c) & 4u) != 0u);

    /* Same trace but drop the rollback — fails. */
    cos_v85_checker_t c2;
    cos_v85_checker_init(&c2);
    cos_v85_register_responds(&c2, pred_reject, pred_rollback);
    cos_v85_observe(&c2, 0x5ULL);
    cos_v85_observe(&c2, 0x1ULL);     /* reject, pending rollback */
    cos_v85_observe(&c2, 0x5ULL);     /* accept, not rollback */
    CHECK((cos_v85_status(&c2) & 4u) == 0u);
}

/* ================================================================== */
/*  6. Compose truth table                                             */
/* ================================================================== */

static void test_compose(void)
{
    for (uint32_t v84 = 0; v84 < 2; ++v84)
        for (uint32_t v85 = 0; v85 < 2; ++v85)
            CHECK(cos_v85_compose_decision(v84, v85) == (v84 & v85));
}

/* ================================================================== */
/*  7. Large fuzz soak                                                 */
/* ================================================================== */

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x; return x;
}

static void soak(uint64_t seed)
{
    uint64_t r = seed;
    for (int trial = 0; trial < 500; ++trial) {
        cos_v85_checker_t c;
        cos_v85_checker_init(&c);
        cos_v85_register_always(&c, pred_composed_ok);
        cos_v85_register_eventually(&c, pred_halt);
        cos_v85_register_responds(&c, pred_reject, pred_rollback);

        int always_violated = 0;
        int halt_ever = 0;
        int reject_pending = 0;
        int responds_resolved = 0;

        for (int i = 0; i < 200; ++i) {
            uint64_t st = xs(&r) & 0xFULL;
            if ((st & 1ULL) == 0) always_violated = 1;
            if ((st >> 1) & 1ULL) halt_ever = 1;
            if (((st >> 2) & 1ULL) == 0) reject_pending = 1;
            if (reject_pending && ((st >> 3) & 1ULL)) {
                responds_resolved = 1;
                reject_pending = 0;
            }
            cos_v85_observe(&c, st);
        }

        uint32_t status = cos_v85_status(&c);
        uint32_t expect_always      = always_violated ? 0u : 1u;
        uint32_t expect_eventually  = halt_ever ? 1u : 0u;
        uint32_t expect_responds    = (responds_resolved && !reject_pending) ? 1u : 0u;
        uint32_t expected_status =
              (expect_always     & 1u)
            | ((expect_eventually & 1u) << 1)
            | ((expect_responds   & 1u) << 2);

        CHECK(status == expected_status);
    }
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v85 σ-Formal — runtime TLA-style invariant "
               "checker (25-bit composed decision)\n");
        return 0;
    }
    const uint64_t seed = 0x858585858585858FULL;

    test_always_happy();
    test_always_caught();
    test_eventually_unfired();
    test_eventually_fired();
    test_responds_pair();
    test_compose();
    soak(seed);

    printf("v85 σ-Formal: %" PRIu64 " PASS / %" PRIu64 " FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
