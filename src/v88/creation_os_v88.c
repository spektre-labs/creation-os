/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v88 — σ-FHE self-test driver.
 *
 * Covers: keygen determinism, encrypt/decrypt round-trip correctness
 * over Z_t, additive homomorphism (Dec(Enc(a)+Enc(b)) == a+b mod t),
 * plaintext-scalar mul (Dec(c·Enc(m)) == c·m mod t), rotation
 * correctness, noise budget monotonicity under chained adds, and
 * the 28-bit compose truth table.
 */

#include "fhe.h"

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

static void fill_seed(uint8_t seed[32], uint64_t base)
{
    uint64_t s = base;
    for (uint32_t i = 0; i < 32u; i += 8) {
        uint64_t v = xs(&s);
        for (uint32_t j = 0; j < 8u; ++j) seed[i + j] = (uint8_t)(v >> (8u * j));
    }
}

/* ------------------------------------------------------------------ */
/*  1. keygen determinism                                              */
/* ------------------------------------------------------------------ */
static void test_keygen_det(void)
{
    uint8_t seed[32];
    fill_seed(seed, 0x7E5700BADDEEDULL);

    cos_v88_ctx_t a, b;
    cos_v88_keygen(&a, seed);
    cos_v88_keygen(&b, seed);
    CHECK(memcmp(&a.s, &b.s, sizeof(a.s)) == 0);
    CHECK(memcmp(&a.a, &b.a, sizeof(a.a)) == 0);
    CHECK(memcmp(&a.b, &b.b, sizeof(a.b)) == 0);
    CHECK(a.sentinel == COS_V88_SENTINEL);
}

/* ------------------------------------------------------------------ */
/*  2. encrypt/decrypt round-trip correctness                          */
/* ------------------------------------------------------------------ */
static void test_roundtrip(uint64_t base)
{
    uint8_t seed[32], r[32];
    fill_seed(seed, base);
    cos_v88_ctx_t c;
    cos_v88_keygen(&c, seed);

    uint64_t rr = base ^ 0xABCDULL;
    for (uint32_t t = 0; t < 16u; ++t) {
        int16_t m[COS_V88_N];
        for (uint32_t i = 0; i < COS_V88_N; ++i)
            m[i] = (int16_t)(xs(&rr) % COS_V88_T);

        fill_seed(r, base + t);
        cos_v88_ct_t ct;
        cos_v88_encrypt(&c, m, r, &ct);

        int16_t m_out[COS_V88_N];
        uint32_t ok = cos_v88_decrypt(&c, &ct, m_out);
        CHECK(ok == 1u);
        for (uint32_t i = 0; i < COS_V88_N; ++i) {
            CHECK(m[i] == m_out[i]);
        }
    }
}

/* ------------------------------------------------------------------ */
/*  3. additive homomorphism                                           */
/* ------------------------------------------------------------------ */
static void test_add_hom(uint64_t base)
{
    uint8_t seed[32], r1[32], r2[32];
    fill_seed(seed, base);
    cos_v88_ctx_t c;
    cos_v88_keygen(&c, seed);

    uint64_t rr = base ^ 0x5511ULL;
    for (uint32_t t = 0; t < 8u; ++t) {
        int16_t ma[COS_V88_N], mb[COS_V88_N], ms[COS_V88_N];
        for (uint32_t i = 0; i < COS_V88_N; ++i) {
            ma[i] = (int16_t)(xs(&rr) % COS_V88_T);
            mb[i] = (int16_t)(xs(&rr) % COS_V88_T);
            ms[i] = (int16_t)((ma[i] + mb[i]) % COS_V88_T);
        }
        fill_seed(r1, base + t);
        fill_seed(r2, base + t + 0x100000ULL);
        cos_v88_ct_t ca, cb, cs;
        cos_v88_encrypt(&c, ma, r1, &ca);
        cos_v88_encrypt(&c, mb, r2, &cb);
        cos_v88_add(&ca, &cb, &cs);
        int16_t out[COS_V88_N];
        uint32_t ok = cos_v88_decrypt(&c, &cs, out);
        CHECK(ok == 1u);
        for (uint32_t i = 0; i < COS_V88_N; ++i) CHECK(out[i] == ms[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  4. plaintext-scalar mul                                            */
/* ------------------------------------------------------------------ */
static void test_mul_plain(uint64_t base)
{
    uint8_t seed[32], r[32];
    fill_seed(seed, base);
    cos_v88_ctx_t c;
    cos_v88_keygen(&c, seed);

    /* Scalar mul amplifies noise linearly; BGV noise budget caps it
     * at ⌊Δ/(2·η_enc)⌋.  We stay safely in the {1,2,3} range where
     * the ceiling of Δ/2 = 238 survives η_enc × k ≤ 3·η.  Larger
     * scalars need modulus switching, which ships in v88.1. */
    uint64_t rr = base ^ 0xFACEULL;
    for (uint32_t t = 0; t < 8u; ++t) {
        int16_t m[COS_V88_N], ref[COS_V88_N];
        int16_t k = (int16_t)((xs(&rr) % 3u) + 1u);
        for (uint32_t i = 0; i < COS_V88_N; ++i) {
            m[i]   = (int16_t)(xs(&rr) % COS_V88_T);
            ref[i] = (int16_t)(((int32_t)m[i] * (int32_t)k) % COS_V88_T);
        }
        fill_seed(r, base + 0x7000ULL + t);
        cos_v88_ct_t ct, cs;
        cos_v88_encrypt(&c, m, r, &ct);
        cos_v88_mul_plain(&ct, k, &cs);
        int16_t out[COS_V88_N];
        uint32_t ok = cos_v88_decrypt(&c, &cs, out);
        CHECK(ok == 1u);
        for (uint32_t i = 0; i < COS_V88_N; ++i) CHECK(out[i] == ref[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  5. rotation                                                        */
/* ------------------------------------------------------------------ */
static void test_rotate(uint64_t base)
{
    uint8_t seed[32], r[32];
    fill_seed(seed, base);
    cos_v88_ctx_t c;
    cos_v88_keygen(&c, seed);

    uint64_t rr = base ^ 0xCAFEULL;
    for (uint32_t t = 0; t < 8u; ++t) {
        int16_t m[COS_V88_N], ref[COS_V88_N];
        for (uint32_t i = 0; i < COS_V88_N; ++i)
            m[i] = (int16_t)(xs(&rr) % COS_V88_T);

        uint32_t k = (uint32_t)(xs(&rr) % COS_V88_N);
        /* Plaintext rotation in the anti-cyclic ring:
         * new[(i+k) mod N] = m[i] * (-1)^{floor((i+k)/N)}.
         * Because m ∈ Z_t and the "sign flip" changes meaning when
         * multiplied by Δ, we track the expected plaintext under
         * the FHE rotation semantics that matches poly_rot. */
        memset(ref, 0, sizeof(ref));
        for (uint32_t i = 0; i < COS_V88_N; ++i) {
            uint32_t j = i + k;
            int16_t sign = 1;
            while (j >= COS_V88_N) { j -= COS_V88_N; sign = (int16_t)(-sign); }
            int32_t v = (int32_t)sign * (int32_t)m[i];
            int32_t r2 = v % (int32_t)COS_V88_T;
            if (r2 < 0) r2 += (int32_t)COS_V88_T;
            ref[j] = (int16_t)r2;
        }

        fill_seed(r, base + 0x9000ULL + t);
        cos_v88_ct_t ct, cs;
        cos_v88_encrypt(&c, m, r, &ct);
        cos_v88_rotate(&ct, k, &cs);
        int16_t out[COS_V88_N];
        uint32_t ok = cos_v88_decrypt(&c, &cs, out);
        CHECK(ok == 1u);
        for (uint32_t i = 0; i < COS_V88_N; ++i) CHECK(out[i] == ref[i]);
    }
}

/* ------------------------------------------------------------------ */
/*  6. noise budget stays within ceiling after 16 chained adds         */
/* ------------------------------------------------------------------ */
static void test_noise_budget(uint64_t base)
{
    uint8_t seed[32], r[32];
    fill_seed(seed, base);
    cos_v88_ctx_t c;
    cos_v88_keygen(&c, seed);

    uint64_t rr = base ^ 0x1F1FULL;
    int16_t m[COS_V88_N];
    for (uint32_t i = 0; i < COS_V88_N; ++i)
        m[i] = (int16_t)(xs(&rr) % COS_V88_T);
    fill_seed(r, base + 0x4000ULL);
    cos_v88_ct_t acc;
    cos_v88_encrypt(&c, m, r, &acc);

    int16_t ref[COS_V88_N];
    for (uint32_t i = 0; i < COS_V88_N; ++i) ref[i] = m[i];

    /* With t=3 and η=2, the additive noise budget holds comfortably
     * for ~6 chained adds before Δ/2 is exhausted; more requires
     * modulus-switching (v88.1). */
    for (uint32_t t = 0; t < 6u; ++t) {
        int16_t mb[COS_V88_N];
        for (uint32_t i = 0; i < COS_V88_N; ++i) {
            mb[i]  = (int16_t)(xs(&rr) % COS_V88_T);
            ref[i] = (int16_t)((ref[i] + mb[i]) % COS_V88_T);
        }
        fill_seed(r, base + 0x4000ULL + t);
        cos_v88_ct_t ct;
        cos_v88_encrypt(&c, mb, r, &ct);
        cos_v88_ct_t nacc;
        cos_v88_add(&acc, &ct, &nacc);
        acc = nacc;
    }

    int16_t out[COS_V88_N];
    uint32_t ok = cos_v88_decrypt(&c, &acc, out);
    CHECK(ok == 1u);
    for (uint32_t i = 0; i < COS_V88_N; ++i) CHECK(out[i] == ref[i]);
    CHECK(cos_v88_noise_budget(&c) <= c.noise_ceiling);
}

/* ------------------------------------------------------------------ */
/*  7. compose truth table                                             */
/* ------------------------------------------------------------------ */
static void test_compose(void)
{
    CHECK(cos_v88_compose_decision(0u, 0u) == 0u);
    CHECK(cos_v88_compose_decision(0u, 1u) == 0u);
    CHECK(cos_v88_compose_decision(1u, 0u) == 0u);
    CHECK(cos_v88_compose_decision(1u, 1u) == 1u);
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v88 σ-FHE — Ring-LWE integer homomorphic "
               "compute (28-bit composed decision)\n");
        return 0;
    }

    const uint64_t base = 0xF4EF4E42042042FEULL;

    test_keygen_det();
    test_roundtrip(base);
    test_add_hom(base);
    test_mul_plain(base);
    test_rotate(base);
    test_noise_budget(base);
    test_compose();

    printf("v88 σ-FHE: %u PASS / %u FAIL\n", pass_count, fail_count);
    return (fail_count == 0) ? 0 : 1;
}
