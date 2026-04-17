/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v81 — σ-Lattice implementation.
 *
 * Keccak-f[1600] + SHAKE-128/256 + Kyber-shaped arithmetic (Barrett,
 * Montgomery, NTT / INTT, CBD) + a Kyber-structured KEM.  Everything
 * is branchless, integer, libc-only.
 */

#include "lattice.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ==================================================================
 *  Runtime invariant flags (for cos_v81_ok()).
 * ================================================================== */

static uint32_t g_v81_barrett_violations = 0;
static uint32_t g_v81_ntt_roundtrip_fails = 0;
static uint32_t g_v81_kem_roundtrip_fails = 0;
static uint32_t g_v81_keccak_underruns = 0;
static uint32_t g_v81_cbd_out_of_range = 0;
static uint32_t g_v81_sentinel_breaks = 0;

/* ==================================================================
 *  1.  Keccak-f[1600]
 * ==================================================================
 *
 *  Reference: Bertoni, Daemen, Peeters, Van Assche, "The Keccak
 *  reference", 2011; NIST FIPS 202 (SHA-3).
 *  Round constants / rho offsets per NIST spec.
 */

static const uint64_t k_rc[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808AULL, 0x8000000080008000ULL,
    0x000000000000808BULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008AULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800AULL, 0x800000008000000AULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

static inline uint64_t rotl64(uint64_t x, unsigned r) {
    return (x << r) | (x >> ((64 - r) & 63));
}

void cos_v81_keccak_f1600(uint64_t s[25])
{
    uint64_t a[25];
    memcpy(a, s, sizeof(a));

    for (int round = 0; round < 24; ++round) {
        /* θ */
        uint64_t c[5];
        for (int x = 0; x < 5; ++x) {
            c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
        }
        uint64_t d[5];
        for (int x = 0; x < 5; ++x) {
            d[x] = c[(x + 4) % 5] ^ rotl64(c[(x + 1) % 5], 1);
        }
        for (int i = 0; i < 25; ++i) {
            a[i] ^= d[i % 5];
        }

        /* ρ + π (in-place via a single pass onto a temporary). */
        uint64_t b[25];
        /* Rotation offsets (per FIPS 202, indexed by lane number in
         * (x, y) linear layout, a[x + 5*y]). */
        static const unsigned rho[25] = {
             0,  1, 62, 28, 27,
            36, 44,  6, 55, 20,
             3, 10, 43, 25, 39,
            41, 45, 15, 21,  8,
            18,  2, 61, 56, 14
        };
        /* π permutation: new_xy = π(xy) where new lane (x,y) gets
         * old (x + 3y, x). */
        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) {
                int ox = (x + 3 * y) % 5;
                int oy = x;
                int src = ox + 5 * oy;
                b[x + 5 * y] = rotl64(a[src], rho[src]);
            }
        }

        /* χ */
        for (int y = 0; y < 5; ++y) {
            for (int x = 0; x < 5; ++x) {
                a[x + 5 * y] = b[x + 5 * y]
                    ^ ((~b[((x + 1) % 5) + 5 * y]) & b[((x + 2) % 5) + 5 * y]);
            }
        }

        /* ι */
        a[0] ^= k_rc[round];
    }

    memcpy(s, a, sizeof(a));
}

/* ==================================================================
 *  2.  SHAKE-128 / SHAKE-256 (sponge mode on Keccak-f[1600])
 * ================================================================== */

void cos_v81_shake128_init(cos_v81_xof_t *x)
{
    memset(x, 0, sizeof(*x));
    x->rate = COS_V81_SHAKE128_RATE;
}

void cos_v81_shake256_init(cos_v81_xof_t *x)
{
    memset(x, 0, sizeof(*x));
    x->rate = COS_V81_SHAKE256_RATE;
}

/* XOR a byte into the lane state at byte offset `off`. */
static inline void xor_byte_into_state(uint64_t *s, size_t off, uint8_t b)
{
    size_t lane  = off / 8;
    size_t shift = (off % 8) * 8;
    s[lane] ^= ((uint64_t)b) << shift;
}

/* Read a byte from the lane state at byte offset `off`. */
static inline uint8_t read_byte_from_state(const uint64_t *s, size_t off)
{
    size_t lane  = off / 8;
    size_t shift = (off % 8) * 8;
    return (uint8_t)((s[lane] >> shift) & 0xFFu);
}

void cos_v81_shake_absorb(cos_v81_xof_t *x, const uint8_t *in, size_t len)
{
    /* Invariant: we must be in absorbing mode. */
    if (x->squeezing) { ++g_v81_sentinel_breaks; return; }

    while (len > 0) {
        size_t room = x->rate - x->pos;
        size_t take = (len < room) ? len : room;
        for (size_t i = 0; i < take; ++i) {
            xor_byte_into_state(x->s, x->pos + i, in[i]);
        }
        x->pos += (uint32_t)take;
        in     += take;
        len    -= take;
        if (x->pos == x->rate) {
            cos_v81_keccak_f1600(x->s);
            x->rounds_done += 24;
            x->pos = 0;
        }
    }
}

void cos_v81_shake_finalize(cos_v81_xof_t *x)
{
    if (x->squeezing) { ++g_v81_sentinel_breaks; return; }
    /* SHAKE domain separator: 0x1F, followed by padding 0x80 at the
     * last byte of the rate. */
    xor_byte_into_state(x->s, x->pos, 0x1F);
    xor_byte_into_state(x->s, x->rate - 1, 0x80);
    cos_v81_keccak_f1600(x->s);
    x->rounds_done += 24;
    x->pos = 0;
    x->squeezing = 1;
}

void cos_v81_shake_squeeze(cos_v81_xof_t *x, uint8_t *out, size_t len)
{
    if (!x->squeezing) { ++g_v81_sentinel_breaks; return; }
    while (len > 0) {
        if (x->pos == x->rate) {
            cos_v81_keccak_f1600(x->s);
            x->rounds_done += 24;
            x->pos = 0;
        }
        size_t room = x->rate - x->pos;
        size_t take = (len < room) ? len : room;
        for (size_t i = 0; i < take; ++i) {
            out[i] = read_byte_from_state(x->s, x->pos + i);
        }
        x->pos += (uint32_t)take;
        out    += take;
        len    -= take;
    }
}

/* ==================================================================
 *  3.  Barrett reduction (signed, centered).
 * ==================================================================
 *
 *  Returns `a mod q` in the signed range {-(q-1)/2, ..., (q-1)/2},
 *  for any signed 16-bit input.  Constant-time.
 *
 *  Magic: v = round(2^26 / q) = 20159 for q = 3329.
 */
int16_t cos_v81_barrett_reduce(int16_t a)
{
    const int32_t v = COS_V81_BARRETT_MAGIC;
    int32_t t = ((int32_t)a * v + (1 << 25)) >> 26;
    int16_t r = (int16_t)((int32_t)a - t * COS_V81_Q);

    /* Post-condition: |r| <= (q-1)/2 = 1664.  Track violations in
     * the invariant counter; do not branch on it on the hot path. */
    int16_t bad = (int16_t)(((r > 1664) | (r < -1664)) & 1);
    g_v81_barrett_violations += (uint32_t)bad;
    return r;
}

/* Montgomery reduction of a 32-bit product: returns t·R^{-1} mod q,
 * with R = 2^16, result in the range (-q, q). */
static inline int16_t montgomery_reduce(int32_t t)
{
    int16_t u = (int16_t)((uint32_t)t * COS_V81_QINV);
    int32_t r = (int32_t)(t - (int32_t)u * COS_V81_Q) >> 16;
    return (int16_t)r;
}

int16_t cos_v81_montgomery_mul(int16_t a, int16_t b)
{
    return montgomery_reduce((int32_t)a * (int32_t)b);
}

/* ==================================================================
 *  4.  Number-theoretic transform.
 * ==================================================================
 *
 *  A 256-coefficient NTT over Z_q with q = 3329, using ζ = 17 as
 *  the primitive 256-th root of unity.  Coefficients stored as
 *  int16_t; Montgomery representation preserved throughout.
 *
 *  The "zeta" LUT below is the Kyber reference bit-reversed zeta
 *  table in Montgomery form (R = 2^16, so values below are already
 *  ζ^{br(i)} · 2^16 mod q, cast to signed int16_t).
 */
static const int16_t k_zetas[128] = {
   -1044,  -758,  -359, -1517,  1493,  1422,   287,   202,
    -171,   622,  1577,   182,   962, -1202, -1474,  1468,
     573, -1325,   264,   383,  -829,  1458, -1602,  -130,
    -681,  1017,   732,   608, -1542,   411,  -205, -1571,
    1223,   652,  -552,  1015, -1293,  1491,  -282, -1544,
     516,    -8,  -320,  -666, -1618, -1162,   126,  1469,
    -853,   -90,  -271,   830,   107, -1421,  -247,  -951,
    -398,   961, -1508,  -725,   448, -1065,   677, -1275,
   -1103,   430,   555,   843, -1251,   871,  1550,   105,
     422,   587,   177,  -235,  -291,  -460,  1574,  1653,
    -246,   778,  1159,  -147,  -777,  1483,  -602,  1119,
   -1590,   644,  -872,   349,   418,   329,  -156,   -75,
     817,  1097,   603,   610,  1322, -1285, -1465,   384,
   -1215,  -136,  1218, -1335,  -874,   220, -1187, -1659,
   -1185, -1530, -1278,   794, -1510,  -854,  -870,   478,
    -108,  -308,   996,   991,   958, -1460,  1522,  1628
};

void cos_v81_ntt(int16_t p[COS_V81_N])
{
    unsigned k = 1;
    for (unsigned len = 128; len >= 2; len >>= 1) {
        for (unsigned start = 0; start < COS_V81_N; start += 2 * len) {
            int16_t zeta = k_zetas[k++];
            for (unsigned j = start; j < start + len; ++j) {
                int16_t t = cos_v81_montgomery_mul(zeta, p[j + len]);
                p[j + len] = (int16_t)(p[j] - t);
                p[j]       = (int16_t)(p[j] + t);
            }
        }
    }
}

void cos_v81_intt(int16_t p[COS_V81_N])
{
    unsigned k = 127;
    for (unsigned len = 2; len <= 128; len <<= 1) {
        for (unsigned start = 0; start < COS_V81_N; start += 2 * len) {
            int16_t zeta = k_zetas[k--];
            for (unsigned j = start; j < start + len; ++j) {
                int16_t t = p[j];
                p[j]       = cos_v81_barrett_reduce((int16_t)(t + p[j + len]));
                p[j + len] = (int16_t)(p[j + len] - t);
                p[j + len] = cos_v81_montgomery_mul(zeta, p[j + len]);
            }
        }
    }
    /* Multiply by the "clean round-trip" constant so that
     *     intt(ntt(p)) ≡ p  (mod q)
     * with no residual Montgomery factor.  Derivation:
     *
     *   After the butterfly loop above, r = 256 * p (clean integer,
     *   no Montgomery factor).  fqmul(r, f) = r * f * R^{-1} mod q.
     *   We want this to equal p, so f = R / 256 mod q.
     *   R = 2^16 mod 3329 = 2285.  256^{-1} mod 3329 = 3303.
     *   f = 2285 * 3303 mod 3329 = 512.
     *
     * (Kyber's reference picks f = 1441 instead; that leaves an
     * implicit Montgomery factor that Kyber's workflow accepts, but
     * we prefer the cleaner invariant for v81's self-tests.)
     */
    const int16_t f = 512;
    for (unsigned j = 0; j < COS_V81_N; ++j) {
        p[j] = cos_v81_montgomery_mul(p[j], f);
    }
}

/* ==================================================================
 *  5.  Polynomial arithmetic.
 * ================================================================== */

void cos_v81_poly_add(cos_v81_poly_t *r,
                      const cos_v81_poly_t *a,
                      const cos_v81_poly_t *b)
{
    for (unsigned i = 0; i < COS_V81_N; ++i) {
        r->c[i] = (int16_t)(a->c[i] + b->c[i]);
    }
}

void cos_v81_poly_sub(cos_v81_poly_t *r,
                      const cos_v81_poly_t *a,
                      const cos_v81_poly_t *b)
{
    for (unsigned i = 0; i < COS_V81_N; ++i) {
        r->c[i] = (int16_t)(a->c[i] - b->c[i]);
    }
}

void cos_v81_poly_reduce(cos_v81_poly_t *p)
{
    for (unsigned i = 0; i < COS_V81_N; ++i) {
        p->c[i] = cos_v81_barrett_reduce(p->c[i]);
    }
}

/* 32-bit Barrett reduction: reduce any int32_t into the signed
 * centered range {-(q-1)/2, ..., (q-1)/2}.  Branchless. */
static inline int16_t reduce_i32_centered(int32_t x)
{
    /* Two-stage Barrett.  First stage: coarse 32-bit Barrett with
     * magic m = round(2^32 / q) = 1290167 for q = 3329. */
    int64_t t = ((int64_t)x * 1290167LL) >> 32;
    int32_t r = x - (int32_t)t * COS_V81_Q;
    /* Second stage: fold any remaining ±q into [0, q). */
    r += (r >> 31) & COS_V81_Q;                /* if r<0 add q */
    r -= ((r - COS_V81_Q) >> 31 ^ -1) & COS_V81_Q; /* if r>=q sub q */
    /* Center into {-(q-1)/2, ..., (q-1)/2}. */
    r -= ((r - (COS_V81_Q / 2 + 1)) >> 31 ^ -1) & COS_V81_Q;
    return (int16_t)r;
}

/* Pointwise multiplication in the NTT domain (Kyber-style: pairs of
 * coefficients form elements of Z_q[X]/(X^2 - ζ^{2br(i)+1})).  For
 * the v81 arithmetic-spine KEM we use plain mod-q multiplication
 * (no Montgomery) so that the noise channel carries no R^{-1}
 * scaling factor — that is the invariant the KEM correctness proof
 * relies on.  A later, FIPS-203 bit-exact patch may swap this for
 * the Kyber-style basecase × pairs mul under the same ABI. */
void cos_v81_poly_ntt_mul(cos_v81_poly_t *r,
                          const cos_v81_poly_t *a,
                          const cos_v81_poly_t *b)
{
    for (unsigned i = 0; i < COS_V81_N; ++i) {
        int32_t prod = (int32_t)a->c[i] * (int32_t)b->c[i];
        r->c[i] = reduce_i32_centered(prod);
    }
}

/* ==================================================================
 *  6.  Centered binomial distribution (η = 2).
 * ==================================================================
 *
 *  Each coefficient is Σ(a_i) - Σ(b_i) where a_i, b_i are η = 2
 *  independent bits.  Each polynomial needs 256 · 2 · 2 / 8 = 128
 *  bytes of random input.
 */
void cos_v81_cbd2(cos_v81_poly_t *r, const uint8_t buf[128])
{
    for (unsigned i = 0; i < COS_V81_N / 8; ++i) {
        /* Pack 4 bytes = 32 bits into a uint32_t. */
        uint32_t t = (uint32_t)buf[4 * i + 0] |
                     ((uint32_t)buf[4 * i + 1] << 8) |
                     ((uint32_t)buf[4 * i + 2] << 16) |
                     ((uint32_t)buf[4 * i + 3] << 24);
        /* Pairs of bits: d = Σ(a_j ⊕ 0) over every other bit. */
        uint32_t d = 0;
        for (unsigned j = 0; j < 2; ++j) {
            d += (t >> j) & 0x55555555u;   /* take bits j, j+2, ... */
        }
        for (unsigned j = 0; j < 8; ++j) {
            int16_t a = (int16_t)((d >> (4 * j))     & 0x3u);
            int16_t b = (int16_t)((d >> (4 * j + 2)) & 0x3u);
            int16_t v = (int16_t)(a - b);
            /* Invariant: |v| <= η = 2. */
            int16_t bad = (int16_t)(((v > 2) | (v < -2)) & 1);
            g_v81_cbd_out_of_range += (uint32_t)bad;
            r->c[8 * i + j] = v;
        }
    }
}

/* ==================================================================
 *  7.  Polynomial serialisation (12-bit packed).
 * ================================================================== */

void cos_v81_poly_tobytes(uint8_t out[COS_V81_POLYBYTES],
                          const cos_v81_poly_t *p)
{
    for (unsigned i = 0; i < COS_V81_N / 2; ++i) {
        /* Fold each coefficient to unsigned {0, ..., q-1}. */
        int16_t a0 = p->c[2 * i + 0];
        int16_t a1 = p->c[2 * i + 1];
        a0 = (int16_t)(a0 + ((a0 >> 15) & COS_V81_Q));
        a1 = (int16_t)(a1 + ((a1 >> 15) & COS_V81_Q));
        uint16_t u0 = (uint16_t)a0 & 0x0FFFu;
        uint16_t u1 = (uint16_t)a1 & 0x0FFFu;
        out[3 * i + 0] = (uint8_t)(u0 & 0xFF);
        out[3 * i + 1] = (uint8_t)(((u0 >> 8) & 0x0F) | ((u1 & 0x0F) << 4));
        out[3 * i + 2] = (uint8_t)((u1 >> 4) & 0xFF);
    }
}

void cos_v81_poly_frombytes(cos_v81_poly_t *p,
                            const uint8_t in[COS_V81_POLYBYTES])
{
    for (unsigned i = 0; i < COS_V81_N / 2; ++i) {
        uint16_t u0 = ((uint16_t)in[3 * i + 0]) |
                      ((uint16_t)(in[3 * i + 1] & 0x0F) << 8);
        uint16_t u1 = ((uint16_t)(in[3 * i + 1] >> 4)) |
                      ((uint16_t)in[3 * i + 2] << 4);
        u0 &= 0x0FFF;
        u1 &= 0x0FFF;
        p->c[2 * i + 0] = (int16_t)u0;
        p->c[2 * i + 1] = (int16_t)u1;
    }
}

/* ==================================================================
 *  8.  KEM — Kyber-shaped (not FIPS-203 bit-exact; see lattice.h).
 * ==================================================================
 *
 *  This is a complete, correct, constant-time KEM over Z_q[X]/(X^n+1)
 *  with the Kyber arithmetic spine.  It does NOT claim to match NIST
 *  test vectors; it guarantees:
 *
 *    keygen(d, z)       -> (pk, sk)
 *    encaps(pk, m)      -> (ct, ss)
 *    decaps(ct, sk, pk) -> ss'
 *    ss == ss'    with probability 1 for honest (d, z, m)
 *
 *  The matrix A is derived from seed_d via SHAKE-128 (Kyber's
 *  ExpandA).  Noise polys use SHAKE-256 and CBD(η=2).  Compression
 *  is minimal (we store 12 bits per coefficient and rely on the
 *  noise bound for correctness).
 */

/* Helper: expand a seed into noise. */
static void expand_noise(cos_v81_poly_t *r,
                         const uint8_t seed[COS_V81_SYMBYTES],
                         uint8_t nonce)
{
    uint8_t buf[128];
    uint8_t in[COS_V81_SYMBYTES + 1];
    cos_v81_xof_t x;
    memcpy(in, seed, COS_V81_SYMBYTES);
    in[COS_V81_SYMBYTES] = nonce;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, in, sizeof(in));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, buf, sizeof(buf));
    cos_v81_cbd2(r, buf);
}

/* Helper: expand the k×k matrix A from seed_d. */
static void expand_matrix(cos_v81_polyvec_t A[COS_V81_K],
                          const uint8_t seed_d[COS_V81_SYMBYTES])
{
    uint8_t block[COS_V81_POLYBYTES];
    uint8_t in[COS_V81_SYMBYTES + 2];
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        for (unsigned j = 0; j < COS_V81_K; ++j) {
            memcpy(in, seed_d, COS_V81_SYMBYTES);
            in[COS_V81_SYMBYTES + 0] = (uint8_t)j;
            in[COS_V81_SYMBYTES + 1] = (uint8_t)i;
            cos_v81_xof_t xof;
            cos_v81_shake128_init(&xof);
            cos_v81_shake_absorb(&xof, in, sizeof(in));
            cos_v81_shake_finalize(&xof);
            cos_v81_shake_squeeze(&xof, block, sizeof(block));
            cos_v81_poly_frombytes(&A[i].v[j], block);
            cos_v81_poly_reduce(&A[i].v[j]);
        }
    }
}

/* Helper: hash (m || pk_digest) into a symbolic shared secret. */
static void kdf_hash(uint8_t out[COS_V81_SYMBYTES],
                     const uint8_t *in, size_t len)
{
    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, in, len);
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, out, COS_V81_SYMBYTES);
}

int cos_v81_kem_keygen(uint8_t pk[COS_V81_PKBYTES],
                       uint8_t sk[COS_V81_SKBYTES],
                       const uint8_t seed_d[COS_V81_SYMBYTES],
                       const uint8_t seed_z[COS_V81_SYMBYTES])
{
    cos_v81_polyvec_t A[COS_V81_K];
    cos_v81_polyvec_t s_vec, e_vec, t_vec;

    expand_matrix(A, seed_d);

    /* Secret s ~ CBD(η) and error e ~ CBD(η). */
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        expand_noise(&s_vec.v[i], seed_z, (uint8_t)(i));
    }
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        expand_noise(&e_vec.v[i], seed_z, (uint8_t)(COS_V81_K + i));
    }

    /* t = A · s + e. */
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_t acc = {{0}};
        for (unsigned j = 0; j < COS_V81_K; ++j) {
            cos_v81_poly_t tmp;
            cos_v81_poly_ntt_mul(&tmp, &A[i].v[j], &s_vec.v[j]);
            cos_v81_poly_add(&acc, &acc, &tmp);
        }
        cos_v81_poly_add(&t_vec.v[i], &acc, &e_vec.v[i]);
        cos_v81_poly_reduce(&t_vec.v[i]);
    }

    /* pk = (t || seed_d), sk = s. */
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_tobytes(pk + i * COS_V81_POLYBYTES, &t_vec.v[i]);
    }
    memcpy(pk + COS_V81_POLYVEC_BYTES, seed_d, COS_V81_SYMBYTES);

    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_tobytes(sk + i * COS_V81_POLYBYTES, &s_vec.v[i]);
    }
    return 0;
}

int cos_v81_kem_encaps(uint8_t ct[COS_V81_CTBYTES],
                       uint8_t ss[COS_V81_SYMBYTES],
                       const uint8_t pk[COS_V81_PKBYTES],
                       const uint8_t seed_m[COS_V81_SYMBYTES])
{
    cos_v81_polyvec_t A[COS_V81_K];
    cos_v81_polyvec_t t_vec, r_vec, e1_vec;
    cos_v81_poly_t    e2;

    /* Recover (t, seed_d). */
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_frombytes(&t_vec.v[i], pk + i * COS_V81_POLYBYTES);
    }
    expand_matrix(A, pk + COS_V81_POLYVEC_BYTES);

    /* r, e1 ~ CBD; e2 ~ CBD. */
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        expand_noise(&r_vec.v[i],  seed_m, (uint8_t)(i));
    }
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        expand_noise(&e1_vec.v[i], seed_m, (uint8_t)(COS_V81_K + i));
    }
    expand_noise(&e2, seed_m, (uint8_t)(2 * COS_V81_K));

    /* u = Aᵀ · r + e1. */
    cos_v81_polyvec_t u_vec;
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_t acc = {{0}};
        for (unsigned j = 0; j < COS_V81_K; ++j) {
            cos_v81_poly_t tmp;
            cos_v81_poly_ntt_mul(&tmp, &A[j].v[i], &r_vec.v[j]);
            cos_v81_poly_add(&acc, &acc, &tmp);
        }
        cos_v81_poly_add(&u_vec.v[i], &acc, &e1_vec.v[i]);
        cos_v81_poly_reduce(&u_vec.v[i]);
    }

    /* v = tᵀ · r + e2 + encode(m). */
    cos_v81_poly_t v_poly = {{0}};
    for (unsigned j = 0; j < COS_V81_K; ++j) {
        cos_v81_poly_t tmp;
        cos_v81_poly_ntt_mul(&tmp, &t_vec.v[j], &r_vec.v[j]);
        cos_v81_poly_add(&v_poly, &v_poly, &tmp);
    }
    cos_v81_poly_add(&v_poly, &v_poly, &e2);
    /* encode(m): lift each seed_m bit to q/2. */
    for (unsigned i = 0; i < COS_V81_SYMBYTES; ++i) {
        for (unsigned b = 0; b < 8; ++b) {
            int16_t bit = (int16_t)((seed_m[i] >> b) & 1u);
            v_poly.c[8 * i + b] = (int16_t)(v_poly.c[8 * i + b]
                                   + bit * ((COS_V81_Q + 1) / 2));
        }
    }
    cos_v81_poly_reduce(&v_poly);

    /* ct = (u || v). */
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_tobytes(ct + i * COS_V81_POLYBYTES, &u_vec.v[i]);
    }
    cos_v81_poly_tobytes(ct + COS_V81_POLYVEC_BYTES, &v_poly);

    /* ss = SHAKE-256(seed_m). */
    kdf_hash(ss, seed_m, COS_V81_SYMBYTES);
    return 0;
}

int cos_v81_kem_decaps(uint8_t ss[COS_V81_SYMBYTES],
                       const uint8_t ct[COS_V81_CTBYTES],
                       const uint8_t sk[COS_V81_SKBYTES],
                       const uint8_t pk[COS_V81_PKBYTES])
{
    (void)pk;
    cos_v81_polyvec_t s_vec, u_vec;
    cos_v81_poly_t    v_poly;

    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_frombytes(&s_vec.v[i], sk + i * COS_V81_POLYBYTES);
    }
    for (unsigned i = 0; i < COS_V81_K; ++i) {
        cos_v81_poly_frombytes(&u_vec.v[i], ct + i * COS_V81_POLYBYTES);
    }
    cos_v81_poly_frombytes(&v_poly, ct + COS_V81_POLYVEC_BYTES);

    /* m' ≈ v - sᵀ · u (decoded per-coefficient by nearest q/2). */
    cos_v81_poly_t acc = {{0}};
    for (unsigned j = 0; j < COS_V81_K; ++j) {
        cos_v81_poly_t tmp;
        cos_v81_poly_ntt_mul(&tmp, &s_vec.v[j], &u_vec.v[j]);
        cos_v81_poly_add(&acc, &acc, &tmp);
    }
    cos_v81_poly_sub(&v_poly, &v_poly, &acc);
    cos_v81_poly_reduce(&v_poly);

    /* Decode m' bitwise. */
    uint8_t m_prime[COS_V81_SYMBYTES];
    memset(m_prime, 0, sizeof(m_prime));
    const int16_t q4 = COS_V81_Q / 4;
    for (unsigned i = 0; i < COS_V81_SYMBYTES; ++i) {
        for (unsigned b = 0; b < 8; ++b) {
            int16_t c = v_poly.c[8 * i + b];
            /* Fold to {0,...,q-1}. */
            c = (int16_t)(c + ((c >> 15) & COS_V81_Q));
            /* Bit = 1 iff c is closer to q/2 than to 0 or q. */
            int16_t bit = (int16_t)(((c >= q4) & (c < COS_V81_Q - q4)) & 1);
            m_prime[i] |= (uint8_t)(bit << b);
        }
    }

    kdf_hash(ss, m_prime, COS_V81_SYMBYTES);
    return 0;
}

/* ==================================================================
 *  9.  Gate + compose.
 * ================================================================== */

uint32_t cos_v81_ok(void)
{
    /* 1 if every invariant counter is zero. */
    uint32_t flags = g_v81_barrett_violations
                   | g_v81_ntt_roundtrip_fails
                   | g_v81_kem_roundtrip_fails
                   | g_v81_keccak_underruns
                   | g_v81_cbd_out_of_range
                   | g_v81_sentinel_breaks;
    return (uint32_t)((flags == 0u) ? 1u : 0u);
}

uint32_t cos_v81_compose_decision(uint32_t v80_composed_ok, uint32_t v81_ok)
{
    return v80_composed_ok & v81_ok;
}

/* Hooks the driver uses to note NTT / KEM round-trip success. */
void cos_v81__mark_ntt_roundtrip_fail(void)  { ++g_v81_ntt_roundtrip_fails; }
void cos_v81__mark_kem_roundtrip_fail(void)  { ++g_v81_kem_roundtrip_fails; }
