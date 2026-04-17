/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v82 — σ-Stream self-test driver.
 */

#include "stream.h"
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

static void make_salt(uint8_t salt[32], uint64_t seed)
{
    uint64_t s = seed;
    for (int i = 0; i < 32; ++i) salt[i] = (uint8_t)(xs(&s) & 0xFFu);
}

static void make_payload(cos_v82_payload_t *p, uint64_t seed)
{
    uint64_t s = seed;
    for (int i = 0; i < 32; ++i) p->h[i] = (uint8_t)(xs(&s) & 0xFFu);
}

/* ================================================================== */
/*  1. All-ones stream: AND stays 1; digest stable; verify replay      */
/* ================================================================== */

static void test_all_ones(uint64_t seed)
{
    uint8_t salt[32]; make_salt(salt, seed);
    cos_v82_stream_t s;
    cos_v82_stream_init(&s, salt);
    uint8_t h0[32]; cos_v82_stream_digest(&s, h0);

    uint32_t bits = 0x1FFFFFu;  /* all 21 kernels pass */
    uint32_t all_bits_arr[64];
    cos_v82_payload_t payloads[64];
    for (int i = 0; i < 64; ++i) {
        all_bits_arr[i] = bits;
        make_payload(&payloads[i], seed + (uint64_t)i);
        uint32_t admitted = cos_v82_chunk_admit(&s, bits, &payloads[i]);
        CHECK(admitted == 1u);
    }

    CHECK(s.chunk_count == 64u);
    CHECK(s.halt_idx == UINT32_MAX);
    CHECK(s.running_and == 1u);
    CHECK(cos_v82_ok(&s) == 1u);

    /* Digest changed from initial (chain advanced). */
    uint8_t h1[32]; cos_v82_stream_digest(&s, h1);
    CHECK(memcmp(h0, h1, 32) != 0);

    /* Replay: external verifier should land on same head. */
    CHECK(cos_v82_stream_verify(salt, all_bits_arr, payloads, 64, h1) == 1u);

    /* Changing one bit should change the replay head. */
    uint8_t fake_head[32]; memcpy(fake_head, h1, 32); fake_head[0] ^= 1;
    CHECK(cos_v82_stream_verify(salt, all_bits_arr, payloads, 64, fake_head) == 0u);
}

/* ================================================================== */
/*  2. Halt-on-flip: once a zero chunk appears, stream halts           */
/* ================================================================== */

static void test_halt_sticky(uint64_t seed)
{
    uint8_t salt[32]; make_salt(salt, seed + 7);
    cos_v82_stream_t s;
    cos_v82_stream_init(&s, salt);

    const uint32_t ok_bits   = 0x1FFFFFu;
    const uint32_t bad_bits  = 0x1FFFFEu;   /* bit 0 zero */

    cos_v82_payload_t p; make_payload(&p, seed);

    /* 10 chunks: pass, pass, pass, fail, pass, pass, ... */
    for (int i = 0; i < 10; ++i) {
        uint32_t bits = (i == 3) ? bad_bits : ok_bits;
        uint32_t admitted = cos_v82_chunk_admit(&s, bits, &p);
        if (i < 3) {
            CHECK(admitted == 1u);
        } else {
            CHECK(admitted == 0u);   /* halted at i=3 and stays halted */
        }
    }
    CHECK(s.halt_idx == 3u);
    CHECK(s.running_and == 0u);
    CHECK(s.chunk_count == 10u);
    CHECK(s.invariant_violations == 0u);
    CHECK(cos_v82_ok(&s) == 1u);  /* structurally sound even though halted */
}

/* ================================================================== */
/*  3. Determinism: same salt + same sequence → same head              */
/* ================================================================== */

static void test_determinism(uint64_t seed)
{
    uint8_t salt[32]; make_salt(salt, seed + 13);
    cos_v82_stream_t a, b;
    cos_v82_stream_init(&a, salt);
    cos_v82_stream_init(&b, salt);

    uint64_t r = seed;
    for (int i = 0; i < 32; ++i) {
        uint32_t bits = ((uint32_t)xs(&r)) & 0x1FFFFFu;
        cos_v82_payload_t p; make_payload(&p, seed + (uint64_t)i);
        uint32_t aa = cos_v82_chunk_admit(&a, bits, &p);
        uint32_t bb = cos_v82_chunk_admit(&b, bits, &p);
        CHECK(aa == bb);
    }
    uint8_t ha[32], hb[32];
    cos_v82_stream_digest(&a, ha);
    cos_v82_stream_digest(&b, hb);
    CHECK(memcmp(ha, hb, 32) == 0);
    CHECK(a.halt_idx == b.halt_idx);
    CHECK(a.chunk_count == b.chunk_count);
}

/* ================================================================== */
/*  4. AND is monotone: halt_idx equals first zero-bits chunk          */
/* ================================================================== */

static void test_monotone(uint64_t seed)
{
    uint64_t r = seed;
    for (int trial = 0; trial < 100; ++trial) {
        uint8_t salt[32]; make_salt(salt, seed + (uint64_t)trial);
        cos_v82_stream_t s;
        cos_v82_stream_init(&s, salt);

        int first_zero = -1;
        for (int i = 0; i < 32; ++i) {
            uint32_t bits;
            if (xs(&r) & 1u) {
                bits = 0x1FFFFFu;  /* all pass */
            } else {
                bits = 0x1FFFFFu & ~(1u << (xs(&r) % 21u));  /* one off */
                if (first_zero < 0) first_zero = i;
            }
            cos_v82_payload_t p; make_payload(&p, seed + (uint64_t)i);
            cos_v82_chunk_admit(&s, bits, &p);
        }
        if (first_zero < 0) {
            CHECK(s.halt_idx == UINT32_MAX);
            CHECK(s.running_and == 1u);
        } else {
            CHECK(s.halt_idx == (uint32_t)first_zero);
            CHECK(s.running_and == 0u);
        }
    }
}

/* ================================================================== */
/*  5. Compose truth table (22-bit)                                    */
/* ================================================================== */

static void test_compose(void)
{
    for (uint32_t v81 = 0; v81 < 2; ++v81) {
        for (uint32_t v82 = 0; v82 < 2; ++v82) {
            CHECK(cos_v82_compose_decision(v81, v82) == (v81 & v82));
        }
    }
}

/* ================================================================== */
/*  6. Fuzz soak: 2048 random streams × 32 chunks ≈ 65k admits         */
/* ================================================================== */

static void soak(uint64_t seed)
{
    uint64_t r = seed;
    for (int trial = 0; trial < 2048; ++trial) {
        uint8_t salt[32]; make_salt(salt, seed + (uint64_t)trial);
        cos_v82_stream_t s;
        cos_v82_stream_init(&s, salt);

        uint32_t bits_arr[32];
        cos_v82_payload_t payloads[32];
        uint32_t running_and = 1u;
        uint32_t halt_idx = UINT32_MAX;
        for (int i = 0; i < 32; ++i) {
            uint32_t bits = ((uint32_t)xs(&r)) & 0x1FFFFFu;
            bits_arr[i] = bits;
            make_payload(&payloads[i], seed + (uint64_t)(trial * 32 + i));

            uint32_t is_full = (bits == 0x1FFFFFu) ? 1u : 0u;
            uint32_t prev_and = running_and;
            running_and = running_and & is_full;
            if (prev_and == 1u && running_and == 0u) halt_idx = (uint32_t)i;

            uint32_t admitted = cos_v82_chunk_admit(&s, bits, &payloads[i]);
            /* admitted iff: prev_and was 1 AND this chunk's bits are all 1. */
            uint32_t expected = (prev_and == 1u) & is_full;
            CHECK(admitted == expected);
        }

        CHECK(s.halt_idx == halt_idx);
        CHECK(s.running_and == running_and);

        /* Verify head. */
        uint8_t head[32]; cos_v82_stream_digest(&s, head);
        CHECK(cos_v82_stream_verify(salt, bits_arr, payloads, 32, head) == 1u);
    }
}

/* ================================================================== */
/*  main                                                                */
/* ================================================================== */

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--version") == 0) {
        printf("creation_os_v82 σ-Stream — streaming per-chunk composed "
               "decision (22-bit AND, halt-on-flip, SHAKE-256 Merkle chain)\n");
        return 0;
    }
    const uint64_t seed = 0x5E7E8A11CE8D51FULL;

    test_all_ones(seed);
    test_halt_sticky(seed);
    test_determinism(seed);
    test_monotone(seed);
    test_compose();
    soak(seed);

    printf("v82 σ-Stream: %" PRIu64 " PASS / %" PRIu64 " FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
