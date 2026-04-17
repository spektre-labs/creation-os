/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v92 — σ-Titans self-test driver.
 */

#include "titans.h"

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
    cos_v92_mem_t a, b;
    cos_v92_mem_init(&a, 0xC0DE92CAFE92ULL);
    cos_v92_mem_init(&b, 0xC0DE92CAFE92ULL);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V92_SENTINEL);
    CHECK(a.events == 0u);
    CHECK(a.writes == 0u);
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) {
        for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d) CHECK(a.vals[i][d] == 0);
        CHECK(a.usage[i] == 0u);
    }

    cos_v92_mem_t c;
    cos_v92_mem_init(&c, 0xC0DE92CAFE93ULL);
    CHECK(memcmp(&a, &c, sizeof(a)) != 0);
}

/* ------------------------------------------------------------------ */
/*  2. first event (new, surprising) triggers a write                  */
/* ------------------------------------------------------------------ */
static void test_first_write(void)
{
    cos_v92_mem_t m;
    cos_v92_mem_init(&m, 0xA1A1A1A1ULL);

    cos_v92_q16_t key[COS_V92_KEY_DIM], tgt[COS_V92_VAL_DIM];
    for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d) key[d] = (cos_v92_q16_t)(1 << 15);
    for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d) tgt[d] = (cos_v92_q16_t)(1 << 16);

    cos_v92_q16_t S = cos_v92_event(&m, key, tgt);
    CHECK((uint32_t)S >= (uint32_t)COS_V92_SURPRISE_LO);
    CHECK(m.writes == 1u);
    CHECK(m.events == 1u);
    CHECK(cos_v92_ok(&m) == 1u);
}

/* ------------------------------------------------------------------ */
/*  3. re-submit same (key,target) → low surprise, reinforce not write */
/* ------------------------------------------------------------------ */
static void test_reinforce(void)
{
    cos_v92_mem_t m;
    cos_v92_mem_init(&m, 0xBEEFBEEFULL);

    cos_v92_q16_t key[COS_V92_KEY_DIM], tgt[COS_V92_VAL_DIM];
    uint64_t s = 0x1234ULL;
    for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d)
        key[d] = (cos_v92_q16_t)((int32_t)(xs(&s) & 0x7FFFu) - 0x4000);
    for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d)
        tgt[d] = (cos_v92_q16_t)((int32_t)(xs(&s) & 0x7FFFu) - 0x4000);

    /* Initial write. */
    (void)cos_v92_event(&m, key, tgt);
    uint32_t writes_after_first = m.writes;

    /* Re-submit; after write the retrieved value matches target so
     * surprise = 0 → reinforce only. */
    cos_v92_q16_t S2 = cos_v92_event(&m, key, tgt);
    CHECK(S2 == 0);
    CHECK(m.writes == writes_after_first);    /* no new write */

    /* Many reinforcements bump usage above 1. */
    for (uint32_t k = 0; k < 100u; ++k) cos_v92_event(&m, key, tgt);
    uint32_t u_max = 0;
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i)
        if (m.usage[i] > u_max) u_max = m.usage[i];
    CHECK(u_max > 10u);
}

/* ------------------------------------------------------------------ */
/*  4. memory capacity: after 10k random events, usage stays bounded   */
/* ------------------------------------------------------------------ */
static void test_bounded(uint64_t seed)
{
    cos_v92_mem_t m;
    cos_v92_mem_init(&m, seed);
    uint64_t s = seed ^ 0xF00FULL;

    cos_v92_q16_t key[COS_V92_KEY_DIM], tgt[COS_V92_VAL_DIM];
    for (uint32_t t = 0; t < 10000u; ++t) {
        for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d)
            key[d] = (cos_v92_q16_t)((int32_t)(xs(&s) & 0x7FFFu) - 0x4000);
        for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d)
            tgt[d] = (cos_v92_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
        cos_v92_event(&m, key, tgt);
        CHECK(cos_v92_ok(&m) == 1u);
    }
}

/* ------------------------------------------------------------------ */
/*  5. test-time learning: surprise monotonically decreases            */
/* ------------------------------------------------------------------ */
static void test_tt_learning(uint64_t seed)
{
    cos_v92_mem_t m;
    cos_v92_mem_init(&m, seed);

    /* Associate a specific key with a specific target repeatedly. */
    cos_v92_q16_t key[COS_V92_KEY_DIM], tgt[COS_V92_VAL_DIM];
    uint64_t s = seed ^ 0xAAAAULL;
    for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d)
        key[d] = (cos_v92_q16_t)((int32_t)(xs(&s) & 0x7FFFu) - 0x4000);
    for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d)
        tgt[d] = (cos_v92_q16_t)(2 << 16);

    cos_v92_q16_t S_first = cos_v92_event(&m, key, tgt);
    cos_v92_q16_t S_last  = 0;
    for (uint32_t k = 0; k < 200u; ++k) {
        S_last = cos_v92_event(&m, key, tgt);
    }
    CHECK(S_last <= S_first);
    CHECK(S_last == 0);
}

/* ------------------------------------------------------------------ */
/*  6. forget compresses usage                                         */
/* ------------------------------------------------------------------ */
static void test_forget(uint64_t seed)
{
    cos_v92_mem_t m;
    cos_v92_mem_init(&m, seed);
    /* Plant high usage. */
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) m.usage[i] = 1000u;
    cos_v92_forget(&m);
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) {
        CHECK(m.usage[i] < 1000u);
        CHECK(m.usage[i] >= 900u);       /* 1000·31/32 = 968 */
    }
}

/* ------------------------------------------------------------------ */
/*  7. determinism under arbitrary event streams                       */
/* ------------------------------------------------------------------ */
static void test_determinism(uint64_t seed)
{
    cos_v92_mem_t a, b;
    cos_v92_mem_init(&a, seed);
    cos_v92_mem_init(&b, seed);

    uint64_t s = seed ^ 0xC0FEULL;
    cos_v92_q16_t key[COS_V92_KEY_DIM], tgt[COS_V92_VAL_DIM];
    for (uint32_t t = 0; t < 1000u; ++t) {
        for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d)
            key[d] = (cos_v92_q16_t)((int32_t)(xs(&s) & 0x7FFFu) - 0x4000);
        for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d)
            tgt[d] = (cos_v92_q16_t)((int32_t)(xs(&s) & 0xFFFFu) - 0x8000);
        cos_v92_q16_t Sa = cos_v92_event(&a, key, tgt);
        cos_v92_q16_t Sb = cos_v92_event(&b, key, tgt);
        CHECK(Sa == Sb);
    }
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
}

/* ------------------------------------------------------------------ */
/*  8. compose truth table                                             */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v92_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v92_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v92_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v92_compose_decision(1u, 1u) == 1u);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v92 σ-Titans — neural long-term memory with "
               "surprise-gated writes and adaptive forgetting "
               "(32-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0xA110C057177A7511ULL;

    test_init();
    test_first_write();
    test_reinforce();
    test_bounded(seed);
    test_tt_learning(seed);
    test_forget(seed);
    test_determinism(seed);
    test_compose();

    printf("v92 σ-Titans: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
