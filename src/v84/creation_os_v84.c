/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v84 — σ-ZKProof self-test driver.
 */

#include "zkproof.h"
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

static void make_digest(cos_v84_digest_t *d, uint64_t seed)
{
    uint64_t s = seed | 1ULL;
    for (int i = 0; i < 32; ++i) d->h[i] = (uint8_t)(xs(&s) & 0xFFu);
}

/* ================================================================== */
/*  1. Basic commit → finalize → open → verify round-trip              */
/* ================================================================== */

static void test_roundtrip(uint64_t seed, uint32_t L)
{
    cos_v84_prover_t p;
    cos_v84_proof_init(&p);

    cos_v84_digest_t layers[COS_V84_MAX_LAYERS];
    for (uint32_t i = 0; i < L; ++i) {
        make_digest(&layers[i], seed + (uint64_t)i);
        CHECK(cos_v84_commit_layer(&p, &layers[i]) == 1u);
    }

    cos_v84_digest_t root;
    cos_v84_finalize(&p, &root);

    /* Verify opening for every layer. */
    for (uint32_t i = 0; i < L; ++i) {
        cos_v84_opening_t o;
        CHECK(cos_v84_open(&p, i, &o) == 1u);
        CHECK(cos_v84_verify_open(&o) == 1u);
        /* The root in the opening must match the finalized root. */
        CHECK(memcmp(o.root.h, root.h, 32) == 0);
    }
}

/* ================================================================== */
/*  2. Tamper detection: a single bit-flip must fail verify            */
/* ================================================================== */

static void test_tamper(uint64_t seed)
{
    cos_v84_prover_t p;
    cos_v84_proof_init(&p);
    for (uint32_t i = 0; i < 16; ++i) {
        cos_v84_digest_t d;
        make_digest(&d, seed + (uint64_t)i);
        cos_v84_commit_layer(&p, &d);
    }
    cos_v84_digest_t root;
    cos_v84_finalize(&p, &root);

    for (uint32_t i = 0; i < 16; ++i) {
        cos_v84_opening_t o_good;
        CHECK(cos_v84_open(&p, i, &o_good) == 1u);

        /* Flip one bit in the layer digest. */
        cos_v84_opening_t o_bad = o_good;
        o_bad.layer_digest.h[0] ^= 0x01u;
        CHECK(cos_v84_verify_open(&o_bad) == 0u);

        /* Flip one bit in chain_after. */
        o_bad = o_good;
        o_bad.chain_after.h[5] ^= 0x80u;
        CHECK(cos_v84_verify_open(&o_bad) == 0u);

        /* Flip one bit in root. */
        o_bad = o_good;
        o_bad.root.h[31] ^= 0x04u;
        CHECK(cos_v84_verify_open(&o_bad) == 0u);

        /* Flip tail entry (if present). */
        if (o_good.tail_len > 0) {
            o_bad = o_good;
            o_bad.tail[0].h[7] ^= 0x20u;
            CHECK(cos_v84_verify_open(&o_bad) == 0u);
        }
    }
}

/* ================================================================== */
/*  3. Reject commit after finalize                                    */
/* ================================================================== */

static void test_finalize_lock(void)
{
    cos_v84_prover_t p;
    cos_v84_proof_init(&p);
    cos_v84_digest_t d;
    make_digest(&d, 1);
    cos_v84_commit_layer(&p, &d);
    cos_v84_digest_t root;
    cos_v84_finalize(&p, &root);

    CHECK(cos_v84_commit_layer(&p, &d) == 0u);
    CHECK(p.invariant_violations == 1u);
    /* cos_v84_ok returns 0 when invariants have been violated. */
    CHECK(cos_v84_ok(&p) == 0u);
}

/* ================================================================== */
/*  4. Compose truth table                                             */
/* ================================================================== */

static void test_compose(void)
{
    for (uint32_t v83 = 0; v83 < 2; ++v83)
        for (uint32_t v84 = 0; v84 < 2; ++v84)
            CHECK(cos_v84_compose_decision(v83, v84) == (v83 & v84));
}

/* ================================================================== */
/*  5. Soak: 200 random provers × up to 32 layers × full open/verify   */
/* ================================================================== */

static void soak(uint64_t seed)
{
    uint64_t r = seed;
    for (int trial = 0; trial < 200; ++trial) {
        uint32_t L = 1u + (uint32_t)(xs(&r) % 32u);
        test_roundtrip(seed + (uint64_t)trial, L);
    }
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v84 σ-ZKProof — NANOZK-style layerwise "
               "Merkle commit (24-bit composed decision)\n");
        return 0;
    }
    const uint64_t seed = 0x484484484484484ULL;

    test_roundtrip(seed, 8);
    test_roundtrip(seed, 32);
    test_roundtrip(seed, 128);
    test_tamper(seed);
    test_finalize_lock();
    test_compose();
    soak(seed);

    printf("v84 σ-ZKProof: %" PRIu64 " PASS / %" PRIu64 " FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
