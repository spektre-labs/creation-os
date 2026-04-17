/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v87 — σ-SAE self-test driver.
 *
 * Covers: init determinism, Top-K sparsity exactness, encode
 * determinism, ablation of inactive feature is a no-op, ablation of
 * active feature changes reconstruction, attribution determinism,
 * reconstruction error bound, and the 27-bit compose truth table.
 */

#include "sae.h"

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
    cos_v87_sae_t a, b;
    cos_v87_sae_init(&a, 0x5A4E5A4E0DEDULL);
    cos_v87_sae_init(&b, 0x5A4E5A4E0DEDULL);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);
    CHECK(a.sentinel == COS_V87_SENTINEL);
}

/* ------------------------------------------------------------------ */
/*  2. Top-K sparsity: exactly K non-zero entries                      */
/* ------------------------------------------------------------------ */
static void test_topk_exact(uint64_t seed)
{
    cos_v87_sae_t s;
    cos_v87_sae_init(&s, seed);

    uint64_t r = seed;
    cos_v87_q16_t h[COS_V87_DIM];
    for (uint32_t t = 0; t < 1000u; ++t) {
        for (uint32_t d = 0; d < COS_V87_DIM; ++d)
            h[d] = (cos_v87_q16_t)((int32_t)(xs(&r) & 0x3FFFFu) - 0x20000);

        cos_v87_q16_t a[COS_V87_FEAT];
        uint32_t ids[COS_V87_TOPK];
        cos_v87_encode(&s, h, a, ids);

        uint32_t nz = 0;
        for (uint32_t f = 0; f < COS_V87_FEAT; ++f) if (a[f] != 0) ++nz;
        CHECK(nz <= COS_V87_TOPK);
        CHECK(s.last_l0 == COS_V87_TOPK);

        /* ids must be unique and in range. */
        for (uint32_t i = 0; i < COS_V87_TOPK; ++i) {
            CHECK(ids[i] < COS_V87_FEAT);
            for (uint32_t j = i + 1; j < COS_V87_TOPK; ++j) {
                CHECK(ids[i] != ids[j]);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/*  3. encode determinism                                              */
/* ------------------------------------------------------------------ */
static void test_encode_determinism(uint64_t seed)
{
    cos_v87_sae_t s;
    cos_v87_sae_init(&s, seed);
    uint64_t r = seed ^ 0x1234ULL;
    for (uint32_t t = 0; t < 500u; ++t) {
        cos_v87_q16_t h[COS_V87_DIM];
        for (uint32_t d = 0; d < COS_V87_DIM; ++d)
            h[d] = (cos_v87_q16_t)((int32_t)(xs(&r) & 0x7FFFu) - 0x4000);
        cos_v87_q16_t a1[COS_V87_FEAT], a2[COS_V87_FEAT];
        uint32_t id1[COS_V87_TOPK], id2[COS_V87_TOPK];
        cos_v87_encode(&s, h, a1, id1);
        cos_v87_encode(&s, h, a2, id2);
        CHECK(memcmp(a1, a2, sizeof(a1)) == 0);
        CHECK(memcmp(id1, id2, sizeof(id1)) == 0);
    }
}

/* ------------------------------------------------------------------ */
/*  4. ablation of inactive feature is no-op                           */
/* ------------------------------------------------------------------ */
static void test_ablate_inactive(uint64_t seed)
{
    cos_v87_sae_t s;
    cos_v87_sae_init(&s, seed);

    uint64_t r = seed ^ 0xC0FFEEULL;
    for (uint32_t t = 0; t < 500u; ++t) {
        cos_v87_q16_t h[COS_V87_DIM];
        for (uint32_t d = 0; d < COS_V87_DIM; ++d)
            h[d] = (cos_v87_q16_t)((int32_t)(xs(&r) & 0x3FFFu) - 0x2000);

        uint32_t ids[COS_V87_TOPK];
        cos_v87_attribute(&s, h, ids);
        /* pick a feature id NOT in ids */
        uint32_t victim = 0;
        for (uint32_t f = 0; f < COS_V87_FEAT; ++f) {
            int in_topk = 0;
            for (uint32_t k = 0; k < COS_V87_TOPK; ++k)
                if (ids[k] == f) in_topk = 1;
            if (!in_topk) { victim = f; break; }
        }
        cos_v87_q16_t delta = cos_v87_ablate(&s, h, victim);
        CHECK(delta == 0);
    }
}

/* ------------------------------------------------------------------ */
/*  5. ablation of active feature changes reconstruction               */
/* ------------------------------------------------------------------ */
static void test_ablate_active(uint64_t seed)
{
    cos_v87_sae_t s;
    cos_v87_sae_init(&s, seed);
    uint64_t r = seed ^ 0xBEEFULL;

    uint32_t changes = 0, total = 500u;
    for (uint32_t t = 0; t < total; ++t) {
        cos_v87_q16_t h[COS_V87_DIM];
        for (uint32_t d = 0; d < COS_V87_DIM; ++d)
            h[d] = (cos_v87_q16_t)((int32_t)(xs(&r) & 0xFFFFu) - 0x8000);
        uint32_t ids[COS_V87_TOPK];
        cos_v87_attribute(&s, h, ids);
        cos_v87_q16_t delta = cos_v87_ablate(&s, h, ids[0]);
        if (delta != 0) ++changes;
    }
    /* With random activations and a non-trivial decoder, ablating
     * the top feature should almost always shift the reconstruction. */
    CHECK(changes >= (total * 9u) / 10u);
}

/* ------------------------------------------------------------------ */
/*  6. reconstruction error stays within budget on zero input          */
/* ------------------------------------------------------------------ */
static void test_recon_budget(uint64_t seed)
{
    cos_v87_sae_t s;
    cos_v87_sae_init(&s, seed);

    cos_v87_q16_t h[COS_V87_DIM] = {0};
    cos_v87_q16_t a[COS_V87_FEAT];
    cos_v87_q16_t hh[COS_V87_DIM];
    uint32_t ids[COS_V87_TOPK];
    cos_v87_encode(&s, h, a, ids);
    cos_v87_decode(&s, a, hh);
    cos_v87_q16_t err = cos_v87_recon_error(hh, h);
    CHECK((uint32_t)err <= s.recon_budget);
}

/* ------------------------------------------------------------------ */
/*  7. invalid fid → violation                                         */
/* ------------------------------------------------------------------ */
static void test_invalid_fid(void)
{
    cos_v87_sae_t s;
    cos_v87_sae_init(&s, 0xFEEDULL);
    cos_v87_q16_t h[COS_V87_DIM] = {0};
    cos_v87_q16_t d = cos_v87_ablate(&s, h, COS_V87_FEAT + 1u);
    CHECK(d == INT32_MAX);
    CHECK(s.invariant_violations == 1u);
    CHECK(cos_v87_ok(&s) == 0u);
}

/* ------------------------------------------------------------------ */
/*  8. compose truth table                                             */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v87_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v87_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v87_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v87_compose_decision(1u, 1u) == 1u);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v87 σ-SAE — Top-K sparse-autoencoder "
               "mechanistic interpretability plane "
               "(27-bit composed decision)\n");
        return 0;
    }

    const uint64_t seed = 0x5A45BABEC0DE5A4EULL;

    test_init();
    test_topk_exact(seed);
    test_encode_determinism(seed);
    test_ablate_inactive(seed);
    test_ablate_active(seed);
    test_recon_budget(seed);
    test_invalid_fid();
    test_compose();

    printf("v87 σ-SAE: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
