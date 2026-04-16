/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v63 — σ-Cipher implementation.
 *
 * All primitives are implemented from scratch in portable C11 with the
 * following non-negotiables:
 *
 *   - constant-time comparison for tag / point equality
 *   - no secret-dependent branches on the AEAD hot path
 *   - secure_zero before returning from any function that holds an
 *     intermediate key or keystream block
 *   - 64-byte aligned cipher state where allocated
 *   - self-test vectors taken verbatim from RFC 7693, RFC 8439, RFC
 *     7748 and RFC 5869
 *
 * A libsodium / liboqs opt-in is wired via COS_V63_LIBSODIUM=1 and
 * COS_V63_LIBOQS=1 respectively; the default build is dependency-free.
 */

#include "cipher.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static inline uint32_t v63_rotl32(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

static inline uint64_t v63_rotr64(uint64_t x, int n)
{
    return (x >> n) | (x << (64 - n));
}

static inline uint32_t v63_load32_le(const uint8_t *p)
{
    return  (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static inline void v63_store32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static inline uint64_t v63_load64_le(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

static inline void v63_store64_le(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

/* ------------------------------------------------------------------ */
/*  1. BLAKE2b-256 (RFC 7693)                                          */
/* ------------------------------------------------------------------ */

static const uint64_t blake2b_iv[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
    0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
    0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint8_t blake2b_sigma[12][16] = {
    {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 },
    { 14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3 },
    { 11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4 },
    {  7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8 },
    {  9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13 },
    {  2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9 },
    { 12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11 },
    { 13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10 },
    {  6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5 },
    { 10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0 },
    {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 },
    { 14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3 }
};

#define B2B_G(a,b,c,d,x,y) do {                                  \
    v[a] = v[a] + v[b] + (x);                                    \
    v[d] = v63_rotr64(v[d] ^ v[a], 32);                          \
    v[c] = v[c] + v[d];                                          \
    v[b] = v63_rotr64(v[b] ^ v[c], 24);                          \
    v[a] = v[a] + v[b] + (y);                                    \
    v[d] = v63_rotr64(v[d] ^ v[a], 16);                          \
    v[c] = v[c] + v[d];                                          \
    v[b] = v63_rotr64(v[b] ^ v[c], 63);                          \
} while (0)

static void blake2b_compress(cos_v63_blake2b_t *s,
                             const uint8_t      block[128],
                             int                last)
{
    uint64_t v[16];
    uint64_t m[16];

    for (int i = 0; i < 16; ++i) m[i] = v63_load64_le(block + 8 * i);
    for (int i = 0; i < 8;  ++i) v[i] = s->h[i];
    for (int i = 0; i < 8;  ++i) v[8 + i] = blake2b_iv[i];

    v[12] ^= s->t[0];
    v[13] ^= s->t[1];
    if (last) v[14] = ~v[14];

    for (int r = 0; r < 12; ++r) {
        const uint8_t *sg = blake2b_sigma[r];
        B2B_G(0, 4,  8, 12, m[sg[ 0]], m[sg[ 1]]);
        B2B_G(1, 5,  9, 13, m[sg[ 2]], m[sg[ 3]]);
        B2B_G(2, 6, 10, 14, m[sg[ 4]], m[sg[ 5]]);
        B2B_G(3, 7, 11, 15, m[sg[ 6]], m[sg[ 7]]);
        B2B_G(0, 5, 10, 15, m[sg[ 8]], m[sg[ 9]]);
        B2B_G(1, 6, 11, 12, m[sg[10]], m[sg[11]]);
        B2B_G(2, 7,  8, 13, m[sg[12]], m[sg[13]]);
        B2B_G(3, 4,  9, 14, m[sg[14]], m[sg[15]]);
    }
    for (int i = 0; i < 8; ++i) s->h[i] ^= v[i] ^ v[i + 8];
}

static void blake2b_init_param(cos_v63_blake2b_t *s,
                               uint32_t           outlen,
                               uint32_t           keylen)
{
    memset(s, 0, sizeof(*s));
    /* BLAKE2b parameter block folded into the first 8 bytes:
     *   byte 0 = digest length
     *   byte 1 = key length
     *   byte 2 = fanout (1)
     *   byte 3 = depth (1)
     *   bytes 4..7 = leaf length (0)
     */
    uint64_t p0 = (uint64_t)outlen
                | ((uint64_t)keylen << 8)
                | ((uint64_t)1      << 16)
                | ((uint64_t)1      << 24);
    for (int i = 0; i < 8; ++i) s->h[i] = blake2b_iv[i];
    s->h[0] ^= p0;
    s->outlen = outlen;
}

void cos_v63_blake2b_init_256(cos_v63_blake2b_t *s)
{
    blake2b_init_param(s, 32, 0);
}

void cos_v63_blake2b_init_keyed_256(cos_v63_blake2b_t *s,
                                    const uint8_t     *key,
                                    size_t             keylen)
{
    if (keylen > 64) keylen = 64;
    blake2b_init_param(s, 32, (uint32_t)keylen);
    if (keylen > 0) {
        uint8_t padded[128] = {0};
        memcpy(padded, key, keylen);
        cos_v63_blake2b_update(s, padded, 128);
        cos_v63_secure_zero(padded, sizeof(padded));
    }
}

void cos_v63_blake2b_update(cos_v63_blake2b_t *s,
                            const uint8_t     *in,
                            size_t             inlen)
{
    while (inlen > 0) {
        uint32_t fill = 128 - s->buflen;
        if (inlen > fill) {
            memcpy(s->buf + s->buflen, in, fill);
            s->t[0] += 128;
            if (s->t[0] < 128) s->t[1] += 1;
            blake2b_compress(s, s->buf, 0);
            s->buflen = 0;
            in    += fill;
            inlen -= fill;
        } else {
            memcpy(s->buf + s->buflen, in, inlen);
            s->buflen += (uint32_t)inlen;
            in    += inlen;
            inlen  = 0;
        }
    }
}

void cos_v63_blake2b_final(cos_v63_blake2b_t *s, uint8_t out[32])
{
    s->t[0] += s->buflen;
    if (s->t[0] < s->buflen) s->t[1] += 1;
    memset(s->buf + s->buflen, 0, 128 - s->buflen);
    blake2b_compress(s, s->buf, 1);

    uint8_t tmp[64];
    for (int i = 0; i < 8; ++i) v63_store64_le(tmp + 8 * i, s->h[i]);
    memcpy(out, tmp, 32);
    cos_v63_secure_zero(tmp, sizeof(tmp));
    cos_v63_secure_zero(s,   sizeof(*s));
}

void cos_v63_blake2b_256(const uint8_t *in, size_t inlen, uint8_t out[32])
{
    cos_v63_blake2b_t s;
    cos_v63_blake2b_init_256(&s);
    cos_v63_blake2b_update(&s, in, inlen);
    cos_v63_blake2b_final(&s, out);
}

/* ------------------------------------------------------------------ */
/*  2. HKDF-BLAKE2b (RFC 5869, HMAC replaced by keyed BLAKE2b-256)     */
/* ------------------------------------------------------------------ */
/*
 * HMAC-BLAKE2b would be the canonical construction; however RFC 7693
 * §2.9 permits keyed-BLAKE2b in place of HMAC, and we use that here
 * because it is simpler, timing-safe, and cryptographically equivalent
 * for the PRF role (BLAKE2b is designed to accept a key directly).
 *
 *     PRK = KeyedBLAKE2b(key = salt, data = IKM)
 *     T(0) = empty
 *     T(i) = KeyedBLAKE2b(key = PRK, data = T(i-1) || info || i)
 *     OKM  = T(1) || T(2) || ... truncated to L bytes
 */

static void v63_prf_blake2b(const uint8_t *key, size_t keylen,
                            const uint8_t *in,  size_t inlen,
                            uint8_t        out[32])
{
    cos_v63_blake2b_t s;
    cos_v63_blake2b_init_keyed_256(&s, key, keylen);
    cos_v63_blake2b_update(&s, in, inlen);
    cos_v63_blake2b_final(&s, out);
}

void cos_v63_hkdf_extract(const uint8_t *salt, size_t saltlen,
                          const uint8_t *ikm,  size_t ikmlen,
                          uint8_t        prk[32])
{
    /* If salt is absent, use a zeroed 32-byte salt per RFC 5869. */
    uint8_t zeros[32] = {0};
    const uint8_t *s   = salt   ? salt   : zeros;
    size_t         slen= salt   ? saltlen: 32;
    v63_prf_blake2b(s, slen, ikm, ikmlen, prk);
}

int cos_v63_hkdf_expand(const uint8_t *prk,
                        const uint8_t *info, size_t infolen,
                        uint8_t       *out,  size_t outlen)
{
    if (outlen > 255U * 32U) return -1;

    uint8_t  t[32];
    uint8_t  buf[32 + 256 + 1]; /* T(i-1) || info || counter */
    uint32_t off = 0;
    uint32_t produced = 0;
    uint8_t  counter = 0;

    while (produced < outlen) {
        counter += 1;
        off = 0;
        if (counter > 1) {
            memcpy(buf + off, t, 32);
            off += 32;
        }
        if (infolen > 0 && info != NULL) {
            /* infolen clamped; caller passes short context labels */
            if (infolen > 256) return -2;
            memcpy(buf + off, info, infolen);
            off += (uint32_t)infolen;
        }
        buf[off++] = counter;
        v63_prf_blake2b(prk, 32, buf, off, t);

        uint32_t take = 32;
        if (produced + take > outlen) take = (uint32_t)(outlen - produced);
        memcpy(out + produced, t, take);
        produced += take;
    }
    cos_v63_secure_zero(t,   sizeof(t));
    cos_v63_secure_zero(buf, sizeof(buf));
    return 0;
}

int cos_v63_hkdf(const uint8_t *salt, size_t saltlen,
                 const uint8_t *ikm,  size_t ikmlen,
                 const uint8_t *info, size_t infolen,
                 uint8_t       *out,  size_t outlen)
{
    uint8_t prk[32];
    cos_v63_hkdf_extract(salt, saltlen, ikm, ikmlen, prk);
    int rc = cos_v63_hkdf_expand(prk, info, infolen, out, outlen);
    cos_v63_secure_zero(prk, sizeof(prk));
    return rc;
}

/* ------------------------------------------------------------------ */
/*  3. ChaCha20 (RFC 8439)                                             */
/* ------------------------------------------------------------------ */

#define CHACHA_QR(a,b,c,d) do {                       \
    a += b; d ^= a; d = v63_rotl32(d, 16);            \
    c += d; b ^= c; b = v63_rotl32(b, 12);            \
    a += b; d ^= a; d = v63_rotl32(d,  8);            \
    c += d; b ^= c; b = v63_rotl32(b,  7);            \
} while (0)

static void chacha20_core(uint32_t out[16], const uint32_t in[16])
{
    uint32_t x[16];
    for (int i = 0; i < 16; ++i) x[i] = in[i];
    for (int r = 0; r < 10; ++r) {
        CHACHA_QR(x[0], x[4], x[ 8], x[12]);
        CHACHA_QR(x[1], x[5], x[ 9], x[13]);
        CHACHA_QR(x[2], x[6], x[10], x[14]);
        CHACHA_QR(x[3], x[7], x[11], x[15]);
        CHACHA_QR(x[0], x[5], x[10], x[15]);
        CHACHA_QR(x[1], x[6], x[11], x[12]);
        CHACHA_QR(x[2], x[7], x[ 8], x[13]);
        CHACHA_QR(x[3], x[4], x[ 9], x[14]);
    }
    for (int i = 0; i < 16; ++i) out[i] = x[i] + in[i];
}

static void chacha20_setup(uint32_t       state[16],
                           const uint8_t  key[32],
                           uint32_t       counter,
                           const uint8_t  nonce[12])
{
    state[0] = 0x61707865; state[1] = 0x3320646e;
    state[2] = 0x79622d32; state[3] = 0x6b206574;
    for (int i = 0; i < 8; ++i) state[4 + i] = v63_load32_le(key + 4 * i);
    state[12] = counter;
    state[13] = v63_load32_le(nonce + 0);
    state[14] = v63_load32_le(nonce + 4);
    state[15] = v63_load32_le(nonce + 8);
}

void cos_v63_chacha20_block(const uint8_t key[32],
                            uint32_t      counter,
                            const uint8_t nonce[12],
                            uint8_t       out[64])
{
    uint32_t state[16];
    uint32_t blk[16];
    chacha20_setup(state, key, counter, nonce);
    chacha20_core(blk, state);
    for (int i = 0; i < 16; ++i) v63_store32_le(out + 4 * i, blk[i]);
    cos_v63_secure_zero(state, sizeof(state));
    cos_v63_secure_zero(blk,   sizeof(blk));
}

void cos_v63_chacha20_xor(const uint8_t  key[32],
                          uint32_t       counter,
                          const uint8_t  nonce[12],
                          const uint8_t *in,
                          uint8_t       *out,
                          size_t         len)
{
    uint32_t state[16];
    uint32_t blk  [16];
    uint8_t  ks   [64];
    chacha20_setup(state, key, counter, nonce);

    while (len > 0) {
        chacha20_core(blk, state);
        for (int i = 0; i < 16; ++i) v63_store32_le(ks + 4 * i, blk[i]);
        size_t take = (len < 64) ? len : 64;
        for (size_t i = 0; i < take; ++i) out[i] = in[i] ^ ks[i];
        in  += take;
        out += take;
        len -= take;
        state[12] += 1; /* increment counter */
    }
    cos_v63_secure_zero(state, sizeof(state));
    cos_v63_secure_zero(blk,   sizeof(blk));
    cos_v63_secure_zero(ks,    sizeof(ks));
}

/* ------------------------------------------------------------------ */
/*  4. Poly1305 (RFC 8439)                                             */
/* ------------------------------------------------------------------ */
/*
 * Reference 26-bit limb implementation (5 limbs × 26 bits = 130 bits),
 * constant-time.  Not the fastest form on M4 — that would be the 64x64
 * limb variant with __uint128_t — but it is easy to audit and this
 * runtime uses Poly1305 only for authentication, not bulk throughput.
 */

typedef struct {
    uint32_t r[5];
    uint32_t h[5];
    uint32_t pad[4];
    size_t   leftover;
    uint8_t  buf[16];
    uint8_t  final;
} v63_poly_t;

static void v63_poly_init(v63_poly_t *p, const uint8_t key[32])
{
    uint32_t t0 = v63_load32_le(key +  0);
    uint32_t t1 = v63_load32_le(key +  4);
    uint32_t t2 = v63_load32_le(key +  8);
    uint32_t t3 = v63_load32_le(key + 12);

    p->r[0] =  t0                        & 0x3ffffff;
    p->r[1] = ((t0 >> 26) | (t1 <<  6))  & 0x3ffff03;
    p->r[2] = ((t1 >> 20) | (t2 << 12))  & 0x3ffc0ff;
    p->r[3] = ((t2 >> 14) | (t3 << 18))  & 0x3f03fff;
    p->r[4] =  (t3 >>  8)                & 0x00fffff;

    for (int i = 0; i < 5; ++i) p->h[i]   = 0;
    for (int i = 0; i < 4; ++i) p->pad[i] = v63_load32_le(key + 16 + 4 * i);
    p->leftover = 0;
    p->final    = 0;
}

static void v63_poly_blocks(v63_poly_t *p, const uint8_t *m, size_t bytes)
{
    const uint32_t hibit = p->final ? 0 : (1u << 24);
    uint32_t r0 = p->r[0], r1 = p->r[1], r2 = p->r[2],
             r3 = p->r[3], r4 = p->r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = p->h[0], h1 = p->h[1], h2 = p->h[2],
             h3 = p->h[3], h4 = p->h[4];

    while (bytes >= 16) {
        uint32_t t0 = v63_load32_le(m +  0);
        uint32_t t1 = v63_load32_le(m +  4);
        uint32_t t2 = v63_load32_le(m +  8);
        uint32_t t3 = v63_load32_le(m + 12);

        h0 += (t0                       ) & 0x3ffffff;
        h1 += ((t0 >> 26) | (t1 <<  6)  ) & 0x3ffffff;
        h2 += ((t1 >> 20) | (t2 << 12)  ) & 0x3ffffff;
        h3 += ((t2 >> 14) | (t3 << 18)  ) & 0x3ffffff;
        h4 += (t3 >>  8) | hibit;

        uint64_t d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4
                    + (uint64_t)h2 * s3 + (uint64_t)h3 * s2
                    + (uint64_t)h4 * s1;
        uint64_t d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0
                    + (uint64_t)h2 * s4 + (uint64_t)h3 * s3
                    + (uint64_t)h4 * s2;
        uint64_t d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1
                    + (uint64_t)h2 * r0 + (uint64_t)h3 * s4
                    + (uint64_t)h4 * s3;
        uint64_t d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2
                    + (uint64_t)h2 * r1 + (uint64_t)h3 * r0
                    + (uint64_t)h4 * s4;
        uint64_t d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3
                    + (uint64_t)h2 * r2 + (uint64_t)h3 * r1
                    + (uint64_t)h4 * r0;

        uint32_t c;
        c  = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
        d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
        d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
        d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
        d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
        h0 += c * 5;  c = h0 >> 26; h0 &= 0x3ffffff;
        h1 += c;

        m     += 16;
        bytes -= 16;
    }
    p->h[0] = h0; p->h[1] = h1; p->h[2] = h2; p->h[3] = h3; p->h[4] = h4;
}

static void v63_poly_finish(v63_poly_t *p, uint8_t tag[16])
{
    if (p->leftover) {
        size_t i = p->leftover;
        p->buf[i++] = 1;
        for (; i < 16; ++i) p->buf[i] = 0;
        p->final = 1;
        v63_poly_blocks(p, p->buf, 16);
    }

    uint32_t h0 = p->h[0], h1 = p->h[1], h2 = p->h[2],
             h3 = p->h[3], h4 = p->h[4];
    uint32_t c;

    c  = h1 >> 26; h1 &= 0x3ffffff; h2 += c;
    c  = h2 >> 26; h2 &= 0x3ffffff; h3 += c;
    c  = h3 >> 26; h3 &= 0x3ffffff; h4 += c;
    c  = h4 >> 26; h4 &= 0x3ffffff; h0 += c * 5;
    c  = h0 >> 26; h0 &= 0x3ffffff; h1 += c;

    uint32_t g0 = h0 + 5;                     c = g0 >> 26; g0 &= 0x3ffffff;
    uint32_t g1 = h1 + c;                     c = g1 >> 26; g1 &= 0x3ffffff;
    uint32_t g2 = h2 + c;                     c = g2 >> 26; g2 &= 0x3ffffff;
    uint32_t g3 = h3 + c;                     c = g3 >> 26; g3 &= 0x3ffffff;
    uint32_t g4 = h4 + c - (1u << 26);

    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0;
    h1 = (h1 & mask) | g1;
    h2 = (h2 & mask) | g2;
    h3 = (h3 & mask) | g3;
    h4 = (h4 & mask) | g4;

    h0 = (h0      ) | (h1 << 26);
    h1 = (h1 >>  6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 <<  8);

    uint64_t f;
    f = (uint64_t)h0 + p->pad[0];             h0 = (uint32_t)f;
    f = (uint64_t)h1 + p->pad[1] + (f >> 32); h1 = (uint32_t)f;
    f = (uint64_t)h2 + p->pad[2] + (f >> 32); h2 = (uint32_t)f;
    f = (uint64_t)h3 + p->pad[3] + (f >> 32); h3 = (uint32_t)f;

    v63_store32_le(tag +  0, h0);
    v63_store32_le(tag +  4, h1);
    v63_store32_le(tag +  8, h2);
    v63_store32_le(tag + 12, h3);

    cos_v63_secure_zero(p, sizeof(*p));
}

static void v63_poly_update(v63_poly_t *p, const uint8_t *m, size_t bytes)
{
    if (p->leftover) {
        size_t want = 16 - p->leftover;
        if (want > bytes) want = bytes;
        memcpy(p->buf + p->leftover, m, want);
        bytes        -= want;
        m            += want;
        p->leftover  += want;
        if (p->leftover < 16) return;
        v63_poly_blocks(p, p->buf, 16);
        p->leftover = 0;
    }
    if (bytes >= 16) {
        size_t want = bytes & ~((size_t)15);
        v63_poly_blocks(p, m, want);
        m     += want;
        bytes -= want;
    }
    if (bytes) {
        memcpy(p->buf + p->leftover, m, bytes);
        p->leftover += bytes;
    }
}

void cos_v63_poly1305(const uint8_t  key[32],
                      const uint8_t *in,
                      size_t         inlen,
                      uint8_t        tag[16])
{
    v63_poly_t p;
    v63_poly_init(&p, key);
    v63_poly_update(&p, in, inlen);
    v63_poly_finish(&p, tag);
}

/* ------------------------------------------------------------------ */
/*  5. ChaCha20-Poly1305 AEAD (RFC 8439 §2.8)                          */
/* ------------------------------------------------------------------ */

static void v63_aead_mac(const uint8_t  poly_key[32],
                         const uint8_t *aad,    size_t aadlen,
                         const uint8_t *ct,     size_t ctlen,
                         uint8_t        tag[16])
{
    v63_poly_t p;
    v63_poly_init(&p, poly_key);

    static const uint8_t zeros[16] = {0};
    v63_poly_update(&p, aad, aadlen);
    if (aadlen % 16) v63_poly_update(&p, zeros, 16 - (aadlen % 16));
    v63_poly_update(&p, ct, ctlen);
    if (ctlen % 16) v63_poly_update(&p, zeros, 16 - (ctlen % 16));

    uint8_t lens[16];
    v63_store64_le(lens + 0, (uint64_t)aadlen);
    v63_store64_le(lens + 8, (uint64_t)ctlen);
    v63_poly_update(&p, lens, 16);
    v63_poly_finish(&p, tag);
}

void cos_v63_aead_encrypt(const uint8_t  key[32],
                          const uint8_t  nonce[12],
                          const uint8_t *aad, size_t aadlen,
                          const uint8_t *pt,  size_t ptlen,
                          uint8_t       *ct,
                          uint8_t        tag[16])
{
    uint8_t poly_key[64];
    cos_v63_chacha20_block(key, 0, nonce, poly_key);
    cos_v63_chacha20_xor(key, 1, nonce, pt, ct, ptlen);
    v63_aead_mac(poly_key, aad, aadlen, ct, ptlen, tag);
    cos_v63_secure_zero(poly_key, sizeof(poly_key));
}

int cos_v63_aead_decrypt(const uint8_t  key[32],
                         const uint8_t  nonce[12],
                         const uint8_t *aad, size_t aadlen,
                         const uint8_t *ct,  size_t ctlen,
                         const uint8_t  tag[16],
                         uint8_t       *pt)
{
    uint8_t poly_key[64];
    uint8_t expected[16];
    cos_v63_chacha20_block(key, 0, nonce, poly_key);
    v63_aead_mac(poly_key, aad, aadlen, ct, ctlen, expected);
    int ok = cos_v63_ct_eq(expected, tag, 16);
    cos_v63_chacha20_xor(key, 1, nonce, ct, pt, ctlen);
    cos_v63_secure_zero(poly_key, sizeof(poly_key));
    cos_v63_secure_zero(expected, sizeof(expected));
    return ok;
}

/* ------------------------------------------------------------------ */
/*  6. X25519 (RFC 7748)                                               */
/* ------------------------------------------------------------------ */
/*
 * Field arithmetic in 2^255 - 19.  Portable 64-bit implementation
 * using 10 × 25/26-bit signed-limb representation (radix 2^25.5) so
 * we avoid __uint128_t dependencies (M4 has it; some cross-targets
 * don't).  This is the "curve25519-donna" style, constant-time.
 */

typedef int64_t v63_fe[10];

static void v63_fe_0(v63_fe h) { for (int i = 0; i < 10; ++i) h[i] = 0; }
static void v63_fe_1(v63_fe h) { v63_fe_0(h); h[0] = 1; }
static void v63_fe_copy(v63_fe h, const v63_fe f)
{
    for (int i = 0; i < 10; ++i) h[i] = f[i];
}

/* Load 3 little-endian bytes as int64 (for ref10 fe_frombytes). */
static inline int64_t v63_load3(const uint8_t *s)
{
    return  (int64_t)s[0]
         | ((int64_t)s[1] << 8)
         | ((int64_t)s[2] << 16);
}

static inline int64_t v63_load4(const uint8_t *s)
{
    return  (int64_t)s[0]
         | ((int64_t)s[1] << 8)
         | ((int64_t)s[2] << 16)
         | ((int64_t)s[3] << 24);
}

/* ref10 fe_frombytes: 32-byte little-endian → 10 × 25.5-bit limbs.
 * Bit positions per limb: 0, 26, 51, 77, 102, 128, 153, 179, 204, 230. */
static void v63_fe_frombytes(v63_fe h, const uint8_t s[32])
{
    int64_t h0 = v63_load4(s);                              /* bits 0..31  */
    int64_t h1 = v63_load3(s + 4)  << 6;                    /* bits 32..   */
    int64_t h2 = v63_load3(s + 7)  << 5;                    /* bits 56..   */
    int64_t h3 = v63_load3(s + 10) << 3;                    /* bits 80..   */
    int64_t h4 = v63_load3(s + 13) << 2;                    /* bits 104..  */
    int64_t h5 = v63_load4(s + 16);                         /* bits 128..  */
    int64_t h6 = v63_load3(s + 20) << 7;                    /* bits 160..  */
    int64_t h7 = v63_load3(s + 23) << 5;                    /* bits 184..  */
    int64_t h8 = v63_load3(s + 26) << 4;                    /* bits 208..  */
    int64_t h9 = (v63_load3(s + 29) & 0x7fffff) << 2;       /* bits 232..254: mask bit 255 */

    int64_t carry;
    carry = (h9 + ((int64_t)1 << 24)) >> 25; h0 += carry * 19; h9 -= carry * ((int64_t)1 << 25);
    carry = (h1 + ((int64_t)1 << 24)) >> 25; h2 += carry;      h1 -= carry * ((int64_t)1 << 25);
    carry = (h3 + ((int64_t)1 << 24)) >> 25; h4 += carry;      h3 -= carry * ((int64_t)1 << 25);
    carry = (h5 + ((int64_t)1 << 24)) >> 25; h6 += carry;      h5 -= carry * ((int64_t)1 << 25);
    carry = (h7 + ((int64_t)1 << 24)) >> 25; h8 += carry;      h7 -= carry * ((int64_t)1 << 25);

    carry = (h0 + ((int64_t)1 << 25)) >> 26; h1 += carry; h0 -= carry * ((int64_t)1 << 26);
    carry = (h2 + ((int64_t)1 << 25)) >> 26; h3 += carry; h2 -= carry * ((int64_t)1 << 26);
    carry = (h4 + ((int64_t)1 << 25)) >> 26; h5 += carry; h4 -= carry * ((int64_t)1 << 26);
    carry = (h6 + ((int64_t)1 << 25)) >> 26; h7 += carry; h6 -= carry * ((int64_t)1 << 26);
    carry = (h8 + ((int64_t)1 << 25)) >> 26; h9 += carry; h8 -= carry * ((int64_t)1 << 26);

    h[0] = h0; h[1] = h1; h[2] = h2; h[3] = h3; h[4] = h4;
    h[5] = h5; h[6] = h6; h[7] = h7; h[8] = h8; h[9] = h9;
}

static void v63_fe_tobytes(uint8_t s[32], const v63_fe h_in)
{
    v63_fe h;
    v63_fe_copy(h, h_in);

    int64_t q = (19 * h[9] + ((int64_t)1 << 24)) >> 25;
    q = (h[0] + q) >> 26;
    q = (h[1] + q) >> 25;
    q = (h[2] + q) >> 26;
    q = (h[3] + q) >> 25;
    q = (h[4] + q) >> 26;
    q = (h[5] + q) >> 25;
    q = (h[6] + q) >> 26;
    q = (h[7] + q) >> 25;
    q = (h[8] + q) >> 26;
    q = (h[9] + q) >> 25;

    h[0] += 19 * q;

    int64_t carry;
    carry = h[0] >> 26; h[1] += carry; h[0] -= carry * ((int64_t)1 << 26);
    carry = h[1] >> 25; h[2] += carry; h[1] -= carry * ((int64_t)1 << 25);
    carry = h[2] >> 26; h[3] += carry; h[2] -= carry * ((int64_t)1 << 26);
    carry = h[3] >> 25; h[4] += carry; h[3] -= carry * ((int64_t)1 << 25);
    carry = h[4] >> 26; h[5] += carry; h[4] -= carry * ((int64_t)1 << 26);
    carry = h[5] >> 25; h[6] += carry; h[5] -= carry * ((int64_t)1 << 25);
    carry = h[6] >> 26; h[7] += carry; h[6] -= carry * ((int64_t)1 << 26);
    carry = h[7] >> 25; h[8] += carry; h[7] -= carry * ((int64_t)1 << 25);
    carry = h[8] >> 26; h[9] += carry; h[8] -= carry * ((int64_t)1 << 26);
    carry = h[9] >> 25;               h[9] -= carry * ((int64_t)1 << 25);

    int64_t h0 = h[0], h1 = h[1], h2 = h[2], h3 = h[3], h4 = h[4];
    int64_t h5 = h[5], h6 = h[6], h7 = h[7], h8 = h[8], h9 = h[9];

    s[ 0] = (uint8_t)( h0       );
    s[ 1] = (uint8_t)( h0 >>  8 );
    s[ 2] = (uint8_t)( h0 >> 16 );
    s[ 3] = (uint8_t)((h0 >> 24) | (h1 <<  2));
    s[ 4] = (uint8_t)( h1 >>  6 );
    s[ 5] = (uint8_t)( h1 >> 14 );
    s[ 6] = (uint8_t)((h1 >> 22) | (h2 <<  3));
    s[ 7] = (uint8_t)( h2 >>  5 );
    s[ 8] = (uint8_t)( h2 >> 13 );
    s[ 9] = (uint8_t)((h2 >> 21) | (h3 <<  5));
    s[10] = (uint8_t)( h3 >>  3 );
    s[11] = (uint8_t)( h3 >> 11 );
    s[12] = (uint8_t)((h3 >> 19) | (h4 <<  6));
    s[13] = (uint8_t)( h4 >>  2 );
    s[14] = (uint8_t)( h4 >> 10 );
    s[15] = (uint8_t)( h4 >> 18 );
    s[16] = (uint8_t)( h5       );
    s[17] = (uint8_t)( h5 >>  8 );
    s[18] = (uint8_t)( h5 >> 16 );
    s[19] = (uint8_t)((h5 >> 24) | (h6 <<  1));
    s[20] = (uint8_t)( h6 >>  7 );
    s[21] = (uint8_t)( h6 >> 15 );
    s[22] = (uint8_t)((h6 >> 23) | (h7 <<  3));
    s[23] = (uint8_t)( h7 >>  5 );
    s[24] = (uint8_t)( h7 >> 13 );
    s[25] = (uint8_t)((h7 >> 21) | (h8 <<  4));
    s[26] = (uint8_t)( h8 >>  4 );
    s[27] = (uint8_t)( h8 >> 12 );
    s[28] = (uint8_t)((h8 >> 20) | (h9 <<  6));
    s[29] = (uint8_t)( h9 >>  2 );
    s[30] = (uint8_t)( h9 >> 10 );
    s[31] = (uint8_t)( h9 >> 18 );
}

static void v63_fe_add(v63_fe h, const v63_fe f, const v63_fe g)
{
    for (int i = 0; i < 10; ++i) h[i] = f[i] + g[i];
}
static void v63_fe_sub(v63_fe h, const v63_fe f, const v63_fe g)
{
    for (int i = 0; i < 10; ++i) h[i] = f[i] - g[i];
}

/* Constant-time conditional swap: if b = 1, swap; b ∈ {0,1}. */
static void v63_fe_cswap(v63_fe f, v63_fe g, int b)
{
    int64_t mask = -(int64_t)b;
    for (int i = 0; i < 10; ++i) {
        int64_t x = mask & (f[i] ^ g[i]);
        f[i] ^= x;
        g[i] ^= x;
    }
}

/* Multiply + carry (adapted from ref10). */
static void v63_fe_mul(v63_fe h, const v63_fe f, const v63_fe g)
{
    int64_t f0 = f[0], f1 = f[1], f2 = f[2], f3 = f[3], f4 = f[4];
    int64_t f5 = f[5], f6 = f[6], f7 = f[7], f8 = f[8], f9 = f[9];
    int64_t g0 = g[0], g1 = g[1], g2 = g[2], g3 = g[3], g4 = g[4];
    int64_t g5 = g[5], g6 = g[6], g7 = g[7], g8 = g[8], g9 = g[9];
    int64_t g1_19 = 19 * g1, g2_19 = 19 * g2, g3_19 = 19 * g3;
    int64_t g4_19 = 19 * g4, g5_19 = 19 * g5, g6_19 = 19 * g6;
    int64_t g7_19 = 19 * g7, g8_19 = 19 * g8, g9_19 = 19 * g9;
    int64_t f1_2 = 2 * f1, f3_2 = 2 * f3, f5_2 = 2 * f5;
    int64_t f7_2 = 2 * f7, f9_2 = 2 * f9;

    int64_t h0 = f0*g0 + f1_2*g9_19 + f2*g8_19 + f3_2*g7_19
               + f4*g6_19 + f5_2*g5_19 + f6*g4_19 + f7_2*g3_19
               + f8*g2_19 + f9_2*g1_19;
    int64_t h1 = f0*g1 + f1*g0 + f2*g9_19 + f3*g8_19 + f4*g7_19
               + f5*g6_19 + f6*g5_19 + f7*g4_19 + f8*g3_19 + f9*g2_19;
    int64_t h2 = f0*g2 + f1_2*g1 + f2*g0 + f3_2*g9_19 + f4*g8_19
               + f5_2*g7_19 + f6*g6_19 + f7_2*g5_19 + f8*g4_19 + f9_2*g3_19;
    int64_t h3 = f0*g3 + f1*g2 + f2*g1 + f3*g0 + f4*g9_19
               + f5*g8_19 + f6*g7_19 + f7*g6_19 + f8*g5_19 + f9*g4_19;
    int64_t h4 = f0*g4 + f1_2*g3 + f2*g2 + f3_2*g1 + f4*g0
               + f5_2*g9_19 + f6*g8_19 + f7_2*g7_19 + f8*g6_19 + f9_2*g5_19;
    int64_t h5 = f0*g5 + f1*g4 + f2*g3 + f3*g2 + f4*g1 + f5*g0
               + f6*g9_19 + f7*g8_19 + f8*g7_19 + f9*g6_19;
    int64_t h6 = f0*g6 + f1_2*g5 + f2*g4 + f3_2*g3 + f4*g2
               + f5_2*g1 + f6*g0 + f7_2*g9_19 + f8*g8_19 + f9_2*g7_19;
    int64_t h7 = f0*g7 + f1*g6 + f2*g5 + f3*g4 + f4*g3 + f5*g2
               + f6*g1 + f7*g0 + f8*g9_19 + f9*g8_19;
    int64_t h8 = f0*g8 + f1_2*g7 + f2*g6 + f3_2*g5 + f4*g4 + f5_2*g3
               + f6*g2 + f7_2*g1 + f8*g0 + f9_2*g9_19;
    int64_t h9 = f0*g9 + f1*g8 + f2*g7 + f3*g6 + f4*g5 + f5*g4
               + f6*g3 + f7*g2 + f8*g1 + f9*g0;

    int64_t carry;
    carry = (h0 + ((int64_t)1 << 25)) >> 26; h1 += carry; h0 -= carry * ((int64_t)1 << 26);
    carry = (h4 + ((int64_t)1 << 25)) >> 26; h5 += carry; h4 -= carry * ((int64_t)1 << 26);
    carry = (h1 + ((int64_t)1 << 24)) >> 25; h2 += carry; h1 -= carry * ((int64_t)1 << 25);
    carry = (h5 + ((int64_t)1 << 24)) >> 25; h6 += carry; h5 -= carry * ((int64_t)1 << 25);
    carry = (h2 + ((int64_t)1 << 25)) >> 26; h3 += carry; h2 -= carry * ((int64_t)1 << 26);
    carry = (h6 + ((int64_t)1 << 25)) >> 26; h7 += carry; h6 -= carry * ((int64_t)1 << 26);
    carry = (h3 + ((int64_t)1 << 24)) >> 25; h4 += carry; h3 -= carry * ((int64_t)1 << 25);
    carry = (h7 + ((int64_t)1 << 24)) >> 25; h8 += carry; h7 -= carry * ((int64_t)1 << 25);
    carry = (h4 + ((int64_t)1 << 25)) >> 26; h5 += carry; h4 -= carry * ((int64_t)1 << 26);
    carry = (h8 + ((int64_t)1 << 25)) >> 26; h9 += carry; h8 -= carry * ((int64_t)1 << 26);
    carry = (h9 + ((int64_t)1 << 24)) >> 25; h0 += carry * 19; h9 -= carry * ((int64_t)1 << 25);
    carry = (h0 + ((int64_t)1 << 25)) >> 26; h1 += carry; h0 -= carry * ((int64_t)1 << 26);

    h[0] = h0; h[1] = h1; h[2] = h2; h[3] = h3; h[4] = h4;
    h[5] = h5; h[6] = h6; h[7] = h7; h[8] = h8; h[9] = h9;
}

static void v63_fe_sq(v63_fe h, const v63_fe f) { v63_fe_mul(h, f, f); }

/* Multiply by 121665 — the RFC 7748 Curve25519 a24 constant =
 * (486662 - 2) / 4 = 121665. */
static void v63_fe_mul121665(v63_fe h, const v63_fe f)
{
    int64_t h0 = f[0] * 121665, h1 = f[1] * 121665, h2 = f[2] * 121665,
            h3 = f[3] * 121665, h4 = f[4] * 121665, h5 = f[5] * 121665,
            h6 = f[6] * 121665, h7 = f[7] * 121665, h8 = f[8] * 121665,
            h9 = f[9] * 121665;
    int64_t carry;
    carry = (h9 + ((int64_t)1 << 24)) >> 25; h0 += carry * 19; h9 -= carry * ((int64_t)1 << 25);
    carry = (h1 + ((int64_t)1 << 24)) >> 25; h2 += carry; h1 -= carry * ((int64_t)1 << 25);
    carry = (h3 + ((int64_t)1 << 24)) >> 25; h4 += carry; h3 -= carry * ((int64_t)1 << 25);
    carry = (h5 + ((int64_t)1 << 24)) >> 25; h6 += carry; h5 -= carry * ((int64_t)1 << 25);
    carry = (h7 + ((int64_t)1 << 24)) >> 25; h8 += carry; h7 -= carry * ((int64_t)1 << 25);
    carry = (h0 + ((int64_t)1 << 25)) >> 26; h1 += carry; h0 -= carry * ((int64_t)1 << 26);
    carry = (h2 + ((int64_t)1 << 25)) >> 26; h3 += carry; h2 -= carry * ((int64_t)1 << 26);
    carry = (h4 + ((int64_t)1 << 25)) >> 26; h5 += carry; h4 -= carry * ((int64_t)1 << 26);
    carry = (h6 + ((int64_t)1 << 25)) >> 26; h7 += carry; h6 -= carry * ((int64_t)1 << 26);
    carry = (h8 + ((int64_t)1 << 25)) >> 26; h9 += carry; h8 -= carry * ((int64_t)1 << 26);
    h[0] = h0; h[1] = h1; h[2] = h2; h[3] = h3; h[4] = h4;
    h[5] = h5; h[6] = h6; h[7] = h7; h[8] = h8; h[9] = h9;
}

/* Field inversion: out = z^(2^255 - 21) (= z^(-1) mod p). */
static void v63_fe_invert(v63_fe out, const v63_fe z)
{
    v63_fe t0, t1, t2, t3;
    v63_fe_sq(t0, z);                               /* z^2       */
    v63_fe_sq(t1, t0);  v63_fe_sq(t1, t1);          /* z^8       */
    v63_fe_mul(t1, z, t1);                          /* z^9       */
    v63_fe_mul(t0, t0, t1);                         /* z^11      */
    v63_fe_sq(t2, t0);                              /* z^22      */
    v63_fe_mul(t1, t1, t2);                         /* z^(2^5-1) */
    v63_fe_sq(t2, t1); for (int i = 1; i < 5;   ++i) v63_fe_sq(t2, t2);
    v63_fe_mul(t1, t2, t1);
    v63_fe_sq(t2, t1); for (int i = 1; i < 10;  ++i) v63_fe_sq(t2, t2);
    v63_fe_mul(t2, t2, t1);
    v63_fe_sq(t3, t2); for (int i = 1; i < 20;  ++i) v63_fe_sq(t3, t3);
    v63_fe_mul(t2, t3, t2);
    v63_fe_sq(t2, t2); for (int i = 1; i < 10;  ++i) v63_fe_sq(t2, t2);
    v63_fe_mul(t1, t2, t1);
    v63_fe_sq(t2, t1); for (int i = 1; i < 50;  ++i) v63_fe_sq(t2, t2);
    v63_fe_mul(t2, t2, t1);
    v63_fe_sq(t3, t2); for (int i = 1; i < 100; ++i) v63_fe_sq(t3, t3);
    v63_fe_mul(t2, t3, t2);
    v63_fe_sq(t2, t2); for (int i = 1; i < 50;  ++i) v63_fe_sq(t2, t2);
    v63_fe_mul(t1, t2, t1);
    v63_fe_sq(t1, t1); for (int i = 1; i < 5;   ++i) v63_fe_sq(t1, t1);
    v63_fe_mul(out, t1, t0);
}

int cos_v63_x25519(uint8_t        out[32],
                   const uint8_t  scalar[32],
                   const uint8_t  u_point[32])
{
    uint8_t e[32];
    memcpy(e, scalar, 32);
    /* RFC 7748 scalar clamp */
    e[ 0] &= 248;
    e[31] &= 127;
    e[31] |= 64;

    /* RFC 7748: mask top bit of u */
    uint8_t u_masked[32];
    memcpy(u_masked, u_point, 32);
    u_masked[31] &= 0x7f;

    v63_fe x1, x2, z2, x3, z3, t0, t1;
    v63_fe_frombytes(x1, u_masked);
    v63_fe_1(x2); v63_fe_0(z2);
    v63_fe_copy(x3, x1); v63_fe_1(z3);

    unsigned swap = 0;
    for (int t = 254; t >= 0; --t) {
        unsigned bit = (e[t >> 3] >> (t & 7)) & 1;
        swap ^= bit;
        v63_fe_cswap(x2, x3, (int)swap);
        v63_fe_cswap(z2, z3, (int)swap);
        swap = bit;

        v63_fe A, AA, B, BB, E, C, D, DA, CB;
        v63_fe_add(A,  x2, z2);
        v63_fe_sq (AA, A);
        v63_fe_sub(B,  x2, z2);
        v63_fe_sq (BB, B);
        v63_fe_sub(E,  AA, BB);
        v63_fe_add(C,  x3, z3);
        v63_fe_sub(D,  x3, z3);
        v63_fe_mul(DA, D, A);
        v63_fe_mul(CB, C, B);

        v63_fe t_add, t_sub;
        v63_fe_add(t_add, DA, CB);
        v63_fe_sub(t_sub, DA, CB);
        v63_fe_sq (x3, t_add);
        v63_fe_sq (t0, t_sub);
        v63_fe_mul(z3, x1, t0);

        v63_fe_mul(x2, AA, BB);
        v63_fe_mul121665(t1, E);
        v63_fe_add(t0, AA, t1);
        v63_fe_mul(z2, E, t0);
    }
    v63_fe_cswap(x2, x3, (int)swap);
    v63_fe_cswap(z2, z3, (int)swap);

    v63_fe inv;
    v63_fe_invert(inv, z2);
    v63_fe_mul(x2, x2, inv);
    v63_fe_tobytes(out, x2);

    /* Reject contributory-zero shared secrets. */
    uint8_t accum = 0;
    for (int i = 0; i < 32; ++i) accum |= out[i];
    cos_v63_secure_zero(e,        sizeof(e));
    cos_v63_secure_zero(u_masked, sizeof(u_masked));
    return accum != 0;
}

void cos_v63_x25519_base(uint8_t pk[32], const uint8_t sk[32])
{
    static const uint8_t base[32] = {9};
    (void)cos_v63_x25519(pk, sk, base);
}

/* ------------------------------------------------------------------ */
/*  7. Constant-time utilities                                         */
/* ------------------------------------------------------------------ */

int cos_v63_ct_eq(const void *a, const void *b, size_t n)
{
    const volatile uint8_t *x = (const volatile uint8_t *)a;
    const volatile uint8_t *y = (const volatile uint8_t *)b;
    uint8_t diff = 0;
    for (size_t i = 0; i < n; ++i) diff |= x[i] ^ y[i];
    /* Map nonzero → 0, zero → 1; branchless. */
    return (int)((1u & ((uint32_t)diff - 1) >> 8));
}

void cos_v63_secure_zero(void *p, size_t n)
{
    volatile uint8_t *q = (volatile uint8_t *)p;
    while (n--) *q++ = 0;
}

/* ------------------------------------------------------------------ */
/*  8. Attestation-bound sealed envelope                               */
/* ------------------------------------------------------------------ */

static void v63_derive_seal_key(const uint8_t  quote256[32],
                                const uint8_t  nonce12 [12],
                                const uint8_t *context,  size_t ctxlen,
                                uint8_t        key_out[32])
{
    /* HKDF:
     *   salt = quote256
     *   IKM  = nonce12 (per-envelope entropy)
     *   info = "cos/v63/seal/" || context
     */
    uint8_t info[16 + 256];
    static const char label[] = "cos/v63/seal/";
    size_t lablen = sizeof(label) - 1;
    size_t take = ctxlen;
    if (take > sizeof(info) - lablen) take = sizeof(info) - lablen;
    memcpy(info, label, lablen);
    if (take) memcpy(info + lablen, context, take);

    (void)cos_v63_hkdf(quote256, 32,
                       nonce12, 12,
                       info, lablen + take,
                       key_out, 32);
    cos_v63_secure_zero(info, sizeof(info));
}

int cos_v63_seal(const uint8_t  quote256[32],
                 const uint8_t  nonce12 [12],
                 const uint8_t *context,  size_t ctxlen,
                 const uint8_t *aad,      size_t aadlen,
                 const uint8_t *pt,       size_t ptlen,
                 uint8_t       *out,      size_t outcap)
{
    if (out == NULL) return -1;
    if (aadlen > UINT32_MAX || ptlen > UINT32_MAX) return -1;
    size_t need = sizeof(cos_v63_envelope_hdr_t) + aadlen + ptlen;
    if (outcap < need) return -1;

    cos_v63_envelope_hdr_t *hdr = (cos_v63_envelope_hdr_t *)out;
    memcpy(hdr->quote256, quote256, 32);
    memcpy(hdr->nonce12,  nonce12,  12);
    hdr->ptlen  = (uint32_t)ptlen;
    hdr->aadlen = (uint32_t)aadlen;

    uint8_t *body = out + sizeof(*hdr);
    if (aadlen) memcpy(body, aad, aadlen);
    uint8_t *ctp = body + aadlen;

    uint8_t key[32];
    v63_derive_seal_key(quote256, nonce12, context, ctxlen, key);

    /* AAD authenticated = [ header-excl-tag ] || caller aad. */
    uint8_t auth_aad[sizeof(cos_v63_envelope_hdr_t) + 256];
    size_t  auth_aad_len = 0;
    memcpy(auth_aad, hdr, sizeof(*hdr));
    /* Zero tag field in auth AAD copy before it's populated. */
    memset(auth_aad + offsetof(cos_v63_envelope_hdr_t, tag16), 0, 16);
    auth_aad_len = sizeof(*hdr);
    if (aadlen) {
        size_t take = (aadlen > 256) ? 256 : aadlen;
        memcpy(auth_aad + auth_aad_len, aad, take);
        auth_aad_len += take;
    }

    cos_v63_aead_encrypt(key, nonce12,
                         auth_aad, auth_aad_len,
                         pt, ptlen,
                         ctp, hdr->tag16);
    cos_v63_secure_zero(key, sizeof(key));
    cos_v63_secure_zero(auth_aad, sizeof(auth_aad));
    return (int)need;
}

int cos_v63_open(const uint8_t *envelope, size_t envlen,
                 const uint8_t *context,  size_t ctxlen,
                 uint8_t       *out_pt,   size_t out_cap)
{
    if (envlen < sizeof(cos_v63_envelope_hdr_t)) return -1;
    const cos_v63_envelope_hdr_t *hdr =
        (const cos_v63_envelope_hdr_t *)envelope;
    size_t need = sizeof(*hdr) + hdr->aadlen + hdr->ptlen;
    if (envlen < need)                return -1;
    if (out_cap < (size_t)hdr->ptlen) return -1;

    const uint8_t *body = envelope + sizeof(*hdr);
    const uint8_t *aad  = body;
    const uint8_t *ctp  = body + hdr->aadlen;

    uint8_t key[32];
    v63_derive_seal_key(hdr->quote256, hdr->nonce12,
                        context, ctxlen, key);

    uint8_t auth_aad[sizeof(cos_v63_envelope_hdr_t) + 256];
    size_t  auth_aad_len = 0;
    memcpy(auth_aad, hdr, sizeof(*hdr));
    memset(auth_aad + offsetof(cos_v63_envelope_hdr_t, tag16), 0, 16);
    auth_aad_len = sizeof(*hdr);
    if (hdr->aadlen) {
        size_t take = (hdr->aadlen > 256) ? 256 : hdr->aadlen;
        memcpy(auth_aad + auth_aad_len, aad, take);
        auth_aad_len += take;
    }

    int ok = cos_v63_aead_decrypt(key, hdr->nonce12,
                                  auth_aad, auth_aad_len,
                                  ctp, hdr->ptlen,
                                  hdr->tag16, out_pt);
    cos_v63_secure_zero(key, sizeof(key));
    cos_v63_secure_zero(auth_aad, sizeof(auth_aad));
    return ok ? (int)hdr->ptlen : -3;
}

/* ------------------------------------------------------------------ */
/*  9. Symmetric ratchet                                               */
/* ------------------------------------------------------------------ */

void cos_v63_ratchet_init(cos_v63_ratchet_t *r, const uint8_t root_key[32])
{
    memcpy(r->chain_key, root_key, 32);
    r->counter = 0;
}

void cos_v63_ratchet_step(cos_v63_ratchet_t *r, uint8_t msg_key_out[32])
{
    /* Branchless KDF-tree step:
     *   new_chain = HKDF(salt = chain, ikm = counter_le, info = "cos/v63/ck")
     *   msg_key   = HKDF(salt = chain, ikm = counter_le, info = "cos/v63/mk")
     */
    uint8_t ctr_le[8];
    v63_store64_le(ctr_le, r->counter);

    uint8_t new_chain[32];
    static const char info_ck[] = "cos/v63/ck";
    static const char info_mk[] = "cos/v63/mk";

    (void)cos_v63_hkdf(r->chain_key, 32,
                       ctr_le, 8,
                       (const uint8_t *)info_ck, sizeof(info_ck) - 1,
                       new_chain, 32);
    (void)cos_v63_hkdf(r->chain_key, 32,
                       ctr_le, 8,
                       (const uint8_t *)info_mk, sizeof(info_mk) - 1,
                       msg_key_out, 32);

    cos_v63_secure_zero(r->chain_key, 32);
    memcpy(r->chain_key, new_chain, 32);
    cos_v63_secure_zero(new_chain, sizeof(new_chain));
    r->counter += 1;
}

/* ------------------------------------------------------------------ */
/*  10. Session handshake (IK-like, single-flight)                     */
/* ------------------------------------------------------------------ */

static void v63_ck_mix(uint8_t ck[32], const uint8_t *in, size_t inlen,
                       uint8_t k_out[32])
{
    uint8_t okm[64];
    (void)cos_v63_hkdf(ck, 32,
                       in, inlen,
                       (const uint8_t *)"cos/v63/hs", 10,
                       okm, 64);
    memcpy(ck,    okm,      32);
    memcpy(k_out, okm + 32, 32);
    cos_v63_secure_zero(okm, sizeof(okm));
}

void cos_v63_session_init(cos_v63_session_t *s,
                          uint8_t            role,
                          const uint8_t      local_sk[32],
                          const uint8_t      remote_pk[32])
{
    memset(s, 0, sizeof(*s));
    memcpy(s->local_sk, local_sk, 32);
    cos_v63_x25519_base(s->local_pk, local_sk);
    if (remote_pk) memcpy(s->remote_pk, remote_pk, 32);
    s->role  = role;
    s->stage = 0;

    /* Protocol hash / chaining-key seed: BLAKE2b("cos/v63/session/v1"). */
    static const char proto[] = "cos/v63/session/v1";
    cos_v63_blake2b_256((const uint8_t *)proto, sizeof(proto) - 1, s->ck);
}

int cos_v63_session_seal_first(cos_v63_session_t *s,
                               const uint8_t *msg,   size_t msglen,
                               uint8_t       *out,   size_t outcap)
{
    if (s->role != 0) return -1;                 /* initiator only */
    if (outcap < 32 + 32 + 16 + msglen + 16) return -1;

    /* Deterministic ephemeral: ephemeral_sk = BLAKE2b(local_sk || "eph"). */
    cos_v63_blake2b_t h;
    cos_v63_blake2b_init_256(&h);
    cos_v63_blake2b_update(&h, s->local_sk, 32);
    cos_v63_blake2b_update(&h, (const uint8_t *)"eph", 3);
    cos_v63_blake2b_final(&h, s->eph_sk);
    s->eph_sk[ 0] &= 248; s->eph_sk[31] &= 127; s->eph_sk[31] |= 64;
    cos_v63_x25519_base(s->eph_pk, s->eph_sk);

    /* IK-like ordering:
     *   1. es = X25519(eph_sk, remote_pk);   ck_mix(es) → k_es
     *   2. encrypt local_pk under k_es        (aad = eph_pk, nonce = 0)
     *   3. ss = X25519(local_sk, remote_pk); ck_mix(ss) → k_ss
     *   4. encrypt msg under k_ss             (aad = "msg", nonce = 1)
     * so the responder can always derive the key for step 2 before it
     * learns the initiator's static public.                            */
    uint8_t es[32], ss[32];
    if (!cos_v63_x25519(es, s->eph_sk, s->remote_pk)) return -2;
    v63_ck_mix(s->ck, es, 32, s->k);

    uint8_t *p = out;
    memcpy(p, s->eph_pk, 32); p += 32;

    uint8_t nonce[12] = {0};
    uint8_t tag1[16];
    cos_v63_aead_encrypt(s->k, nonce, s->eph_pk, 32,
                         s->local_pk, 32, p, tag1);
    p += 32;
    memcpy(p, tag1, 16);
    p += 16;

    if (!cos_v63_x25519(ss, s->local_sk, s->remote_pk)) return -2;
    v63_ck_mix(s->ck, ss, 32, s->k);

    nonce[0] = 1;
    uint8_t tag2[16];
    cos_v63_aead_encrypt(s->k, nonce, (const uint8_t *)"msg", 3,
                         msg, msglen, p, tag2);
    p += msglen;
    memcpy(p, tag2, 16);
    p += 16;

    s->stage = 2;
    cos_v63_secure_zero(es, sizeof(es));
    cos_v63_secure_zero(ss, sizeof(ss));
    return (int)(p - out);
}

int cos_v63_session_open_first(cos_v63_session_t *s,
                               const uint8_t *in,    size_t inlen,
                               uint8_t       *out,   size_t outcap)
{
    if (s->role != 1) return -1;                 /* responder only */
    if (inlen < 32 + 32 + 16 + 16) return -1;

    const uint8_t *eph_pk    = in + 0;
    const uint8_t *ct_pk     = in + 32;
    const uint8_t *tag_pk    = in + 32 + 32;
    const uint8_t *ct_msg    = in + 32 + 32 + 16;
    size_t         msglen    = inlen - (32 + 32 + 16 + 16);
    const uint8_t *tag_msg   = ct_msg + msglen;
    if (outcap < msglen) return -1;

    memcpy(s->eph_pk, eph_pk, 32);

    uint8_t es[32], ss[32];
    if (!cos_v63_x25519(es, s->local_sk, eph_pk))         return -2;

    v63_ck_mix(s->ck, es, 32, s->k);

    /* Under post-es key, decrypt the initiator's static public. */
    uint8_t nonce[12] = {0};
    uint8_t remote_pk[32];
    int ok = cos_v63_aead_decrypt(s->k, nonce, eph_pk, 32,
                                  ct_pk, 32, tag_pk, remote_pk);
    if (!ok) { cos_v63_secure_zero(es, sizeof(es)); return -3; }
    memcpy(s->remote_pk, remote_pk, 32);

    if (!cos_v63_x25519(ss, s->local_sk, remote_pk)) {
        cos_v63_secure_zero(es, sizeof(es)); return -2;
    }
    v63_ck_mix(s->ck, ss, 32, s->k);

    nonce[0] = 1;
    ok = cos_v63_aead_decrypt(s->k, nonce, (const uint8_t *)"msg", 3,
                              ct_msg, msglen, tag_msg, out);
    cos_v63_secure_zero(es, sizeof(es));
    cos_v63_secure_zero(ss, sizeof(ss));
    cos_v63_secure_zero(remote_pk, sizeof(remote_pk));
    if (!ok) return -3;
    s->stage = 2;
    return (int)msglen;
}

/* ------------------------------------------------------------------ */
/*  11. Composition decision                                           */
/* ------------------------------------------------------------------ */

cos_v63_decision_t cos_v63_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok)
{
    cos_v63_decision_t d;
    d.v60_ok = v60_ok & 1;
    d.v61_ok = v61_ok & 1;
    d.v62_ok = v62_ok & 1;
    d.v63_ok = v63_ok & 1;
    d.allow  = d.v60_ok & d.v61_ok & d.v62_ok & d.v63_ok;
    d._pad[0] = d._pad[1] = d._pad[2] = 0;
    return d;
}

/* ------------------------------------------------------------------ */
/*  12. Version                                                        */
/* ------------------------------------------------------------------ */

cos_v63_version_t cos_v63_version(void)
{
    cos_v63_version_t v = { 63, 0, 0 };
    return v;
}

const char *cos_v63_build_info(void)
{
#if defined(COS_V63_LIBSODIUM)
    return "v63.0 sigma-cipher (blake2b+chacha20+poly1305+x25519; libsodium=on)";
#else
    return "v63.0 sigma-cipher (blake2b+chacha20+poly1305+x25519; portable)";
#endif
}
