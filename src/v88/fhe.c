/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v88 — σ-FHE implementation.
 *
 * Ring-LWE with q = 3329, N = 256, CBD(η=2) noise — plaintext
 * alphabet Z_t with t = 7.  Encoding: m ∈ Z_t is embedded in Z_q
 * by Δ = round(q / t) = 476 so that the scaled plaintext centre
 * survives every additive operation whose total noise stays within
 * Δ/2 = 238.
 *
 * SHAKE-256 (from v81) seeds the CBD sampler; keygen/encrypt are
 * deterministic functions of their 32-byte seeds.
 */

#include "fhe.h"
#include "lattice.h"           /* v81 Keccak + SHAKE */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DELTA   ((COS_V88_Q + (COS_V88_T / 2u)) / COS_V88_T)   /* = 476 */

/* ------------------------------------------------------------------ *
 *  Poly arithmetic in Z_q[X]/(X^N + 1) — simple schoolbook (BGV).   *
 * ------------------------------------------------------------------ */

static inline int16_t mod_q(int32_t x)
{
    int32_t r = x % (int32_t)COS_V88_Q;
    if (r < 0) r += (int32_t)COS_V88_Q;
    return (int16_t)r;
}

static void poly_zero(cos_v88_poly_t *p) { memset(p, 0, sizeof(*p)); }

static void poly_add(cos_v88_poly_t *r,
                     const cos_v88_poly_t *a,
                     const cos_v88_poly_t *b)
{
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        r->coeffs[i] = mod_q((int32_t)a->coeffs[i] + (int32_t)b->coeffs[i]);
    }
}

/* r = -a (mod q). */
static void poly_neg(cos_v88_poly_t *r, const cos_v88_poly_t *a)
{
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        r->coeffs[i] = mod_q(-(int32_t)a->coeffs[i]);
    }
}

/* r = a * b in Z_q[X]/(X^N+1). O(N^2) schoolbook; N=256 -> 65k ops. */
static void poly_mul(cos_v88_poly_t *r,
                     const cos_v88_poly_t *a,
                     const cos_v88_poly_t *b)
{
    int32_t tmp[2 * COS_V88_N] = {0};
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        for (uint32_t j = 0; j < COS_V88_N; ++j) {
            tmp[i + j] += (int32_t)a->coeffs[i] * (int32_t)b->coeffs[j];
            tmp[i + j] %= (int32_t)COS_V88_Q;
        }
    }
    /* Reduce modulo X^N + 1: coefficients of X^{N..2N-1} get negated
     * and added to X^{0..N-1}. */
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        int32_t v = tmp[i] - tmp[i + COS_V88_N];
        r->coeffs[i] = mod_q(v);
    }
}

/* r = scalar * a. */
static void poly_mul_scalar(cos_v88_poly_t *r,
                            const cos_v88_poly_t *a,
                            int32_t scalar)
{
    int32_t s = scalar % (int32_t)COS_V88_Q;
    if (s < 0) s += (int32_t)COS_V88_Q;
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        r->coeffs[i] = (int16_t)(((int32_t)a->coeffs[i] * s)
                                 % (int32_t)COS_V88_Q);
    }
}

/* Cyclic rotation by k slots (multiplies by X^k modulo X^N+1). */
static void poly_rot(cos_v88_poly_t *r, const cos_v88_poly_t *a, uint32_t k)
{
    cos_v88_poly_t tmp;
    poly_zero(&tmp);
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        uint32_t j = i + k;
        int16_t  sign = 1;
        while (j >= COS_V88_N) { j -= COS_V88_N; sign = (int16_t)(-sign); }
        int32_t v = (int32_t)sign * (int32_t)a->coeffs[i];
        tmp.coeffs[j] = mod_q((int32_t)tmp.coeffs[j] + v);
    }
    *r = tmp;
}

/* ------------------------------------------------------------------ *
 *  Deterministic samplers from 32-byte seeds                         *
 * ------------------------------------------------------------------ */

/* Sample `a` uniformly from Z_q^N via SHAKE-256 with rejection. */
static void sample_uniform(cos_v88_poly_t *p, const uint8_t seed[32],
                           uint8_t nonce)
{
    uint8_t ext[33];
    memcpy(ext, seed, 32);
    ext[32] = nonce;

    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, ext, sizeof(ext));
    cos_v81_shake_finalize(&x);

    uint32_t got = 0;
    uint8_t  buf[168];
    while (got < COS_V88_N) {
        cos_v81_shake_squeeze(&x, buf, sizeof(buf));
        for (uint32_t i = 0; i + 1u < sizeof(buf) && got < COS_V88_N; i += 2) {
            uint16_t v = (uint16_t)buf[i] | ((uint16_t)(buf[i + 1] & 0x0F) << 8);
            if (v < COS_V88_Q) {
                p->coeffs[got++] = (int16_t)v;
            }
        }
    }
}

/* CBD(η=2): coefficients in {-2, -1, 0, +1, +2}. */
static void sample_cbd(cos_v88_poly_t *p, const uint8_t seed[32],
                       uint8_t nonce)
{
    uint8_t ext[33];
    memcpy(ext, seed, 32);
    ext[32] = nonce;

    uint8_t buf[COS_V88_N];
    cos_v81_xof_t x;
    cos_v81_shake256_init(&x);
    cos_v81_shake_absorb(&x, ext, sizeof(ext));
    cos_v81_shake_finalize(&x);
    cos_v81_shake_squeeze(&x, buf, sizeof(buf));

    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        uint8_t b = buf[i];
        int32_t a = (int32_t)((b >> 0) & 1) + (int32_t)((b >> 1) & 1);
        int32_t c = (int32_t)((b >> 2) & 1) + (int32_t)((b >> 3) & 1);
        p->coeffs[i] = (int16_t)mod_q(a - c);
    }
}

/* ------------------------------------------------------------------ *
 *  Keygen                                                            *
 * ------------------------------------------------------------------ */

void cos_v88_keygen(cos_v88_ctx_t *c, const uint8_t seed[32])
{
    memset(c, 0, sizeof(*c));
    c->sentinel = COS_V88_SENTINEL;
    c->noise_ceiling = (uint32_t)(DELTA / 2u);   /* = 238 */

    sample_uniform(&c->a, seed, 0x00);
    sample_cbd    (&c->s, seed, 0x01);
    cos_v88_poly_t e;
    sample_cbd(&e, seed, 0x02);

    /* b = -a·s + e  (mod q). */
    cos_v88_poly_t as_, nas;
    poly_mul(&as_, &c->a, &c->s);
    poly_neg(&nas, &as_);
    poly_add(&c->b, &nas, &e);

    /* Initial noise is η = 2 per coeff in the key-generation error;
     * starting budget equals the full ceiling. */
    c->noise_budget = 0u;      /* zero operations so far */
}

/* ------------------------------------------------------------------ *
 *  Encrypt                                                           *
 * ------------------------------------------------------------------ */

void cos_v88_encrypt(const cos_v88_ctx_t *c,
                     const int16_t m[COS_V88_N],
                     const uint8_t r[32],
                     cos_v88_ct_t *out)
{
    if (c->sentinel != COS_V88_SENTINEL) return;

    cos_v88_poly_t u, e1, e2;
    sample_cbd(&u,  r, 0x10);
    sample_cbd(&e1, r, 0x11);
    sample_cbd(&e2, r, 0x12);

    cos_v88_poly_t m_delta;
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        int32_t mi = (int32_t)(m[i] % (int32_t)COS_V88_T);
        if (mi < 0) mi += (int32_t)COS_V88_T;
        m_delta.coeffs[i] = (int16_t)(mi * (int32_t)DELTA);
    }

    /* c0 = b·u + e1 + Δ·m,  c1 = a·u + e2. */
    cos_v88_poly_t bu, au;
    poly_mul(&bu, &c->b, &u);
    poly_mul(&au, &c->a, &u);
    cos_v88_poly_t bu_e1;
    poly_add(&bu_e1, &bu, &e1);
    poly_add(&out->c0, &bu_e1, &m_delta);
    poly_add(&out->c1, &au, &e2);
}

/* ------------------------------------------------------------------ *
 *  Decrypt                                                           *
 * ------------------------------------------------------------------ */

uint32_t cos_v88_decrypt(cos_v88_ctx_t *c,
                         const cos_v88_ct_t *ct,
                         int16_t m_out[COS_V88_N])
{
    if (c->sentinel != COS_V88_SENTINEL) {
        ++c->invariant_violations;
        return 0u;
    }

    /* v = c0 + c1·s ≈ Δ·m + e_total  (mod q). */
    cos_v88_poly_t c1s, v;
    poly_mul(&c1s, &ct->c1, &c->s);
    poly_add(&v, &ct->c0, &c1s);

    uint32_t max_err = 0u;
    for (uint32_t i = 0; i < COS_V88_N; ++i) {
        int32_t x = (int32_t)v.coeffs[i];
        /* Centre the coefficient in [-q/2, q/2). */
        if (x >= (int32_t)(COS_V88_Q / 2u)) x -= (int32_t)COS_V88_Q;

        /* Divide by Δ with rounding to nearest integer, then mod t. */
        int32_t sign = (x < 0) ? -1 : 1;
        int32_t ax   = sign * x;
        int32_t mi   = (ax + (int32_t)DELTA / 2) / (int32_t)DELTA;
        mi *= sign;

        /* Residual (error) after re-encoding. */
        int32_t resid = x - mi * (int32_t)DELTA;
        if (resid < 0) resid = -resid;
        if ((uint32_t)resid > max_err) max_err = (uint32_t)resid;

        /* Reduce to Z_t. */
        int32_t mt = mi % (int32_t)COS_V88_T;
        if (mt < 0) mt += (int32_t)COS_V88_T;
        m_out[i] = (int16_t)mt;
    }
    c->noise_budget = max_err;
    c->last_correct = (max_err <= c->noise_ceiling) ? 1u : 0u;
    return c->last_correct;
}

/* ------------------------------------------------------------------ *
 *  Homomorphic operations                                            *
 * ------------------------------------------------------------------ */

void cos_v88_add(const cos_v88_ct_t *a,
                 const cos_v88_ct_t *b,
                 cos_v88_ct_t *out)
{
    poly_add(&out->c0, &a->c0, &b->c0);
    poly_add(&out->c1, &a->c1, &b->c1);
}

void cos_v88_mul_plain(const cos_v88_ct_t *a,
                       int16_t scalar,
                       cos_v88_ct_t *out)
{
    poly_mul_scalar(&out->c0, &a->c0, (int32_t)scalar);
    poly_mul_scalar(&out->c1, &a->c1, (int32_t)scalar);
}

void cos_v88_rotate(const cos_v88_ct_t *a, uint32_t k,
                    cos_v88_ct_t *out)
{
    poly_rot(&out->c0, &a->c0, k);
    poly_rot(&out->c1, &a->c1, k);
}

/* ------------------------------------------------------------------ *
 *  Introspection + gate                                              *
 * ------------------------------------------------------------------ */

uint32_t cos_v88_noise_budget(const cos_v88_ctx_t *c)
{
    return c->noise_budget;
}

uint32_t cos_v88_ok(const cos_v88_ctx_t *c)
{
    uint32_t sent    = (c->sentinel == COS_V88_SENTINEL)   ? 1u : 0u;
    uint32_t noviol  = (c->invariant_violations == 0u)      ? 1u : 0u;
    uint32_t noise_ok= (c->noise_budget <= c->noise_ceiling)? 1u : 0u;
    uint32_t corr    = (c->last_correct != 0u)              ? 1u : 0u;
    return sent & noviol & noise_ok & corr;
}

uint32_t cos_v88_compose_decision(uint32_t v87_composed_ok, uint32_t v88_ok)
{
    return v87_composed_ok & v88_ok;
}
