/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v91 — σ-Quantum implementation.
 */

#include "quantum.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline cos_v91_q16_t q_mul(cos_v91_q16_t a, cos_v91_q16_t b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    return (cos_v91_q16_t)(p >> 16);
}

void cos_v91_reg_init(cos_v91_reg_t *r)
{
    memset(r, 0, sizeof(*r));
    r->sentinel = COS_V91_SENTINEL;
    r->a[0] = COS_V91_Q1;    /* |0000> = 1 */
}

/* Pauli-X: swap amplitudes a[x] <-> a[x XOR bit_i]. */
void cos_v91_x(cos_v91_reg_t *r, uint32_t qubit)
{
    uint32_t m = 1u << qubit;
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        uint32_t y = x ^ m;
        if (x < y) {
            cos_v91_q16_t t = r->a[x];
            r->a[x] = r->a[y];
            r->a[y] = t;
        }
    }
    ++r->ops;
}

/* Pauli-Z: negate amplitudes where qubit bit is 1. */
void cos_v91_z(cos_v91_reg_t *r, uint32_t qubit)
{
    uint32_t m = 1u << qubit;
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        if (x & m) r->a[x] = (cos_v91_q16_t)(-(int32_t)r->a[x]);
    }
    ++r->ops;
}

/* Hadamard on qubit i:
 *   a'[x with bit=0] = (a[x] + a[x XOR m]) / sqrt(2)
 *   a'[x with bit=1] = (a[x XOR m] - a[x]) / sqrt(2)      (when x has bit=1)
 *
 * Equivalently for the pair (x0, x1) with bit=0 at x0:
 *   a'[x0] = (a[x0] + a[x1]) * inv_sqrt2
 *   a'[x1] = (a[x0] - a[x1]) * inv_sqrt2
 */
void cos_v91_h(cos_v91_reg_t *r, uint32_t qubit)
{
    uint32_t m = 1u << qubit;
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        if (x & m) continue;
        uint32_t y = x | m;
        cos_v91_q16_t a0 = r->a[x];
        cos_v91_q16_t a1 = r->a[y];
        int32_t sum  = (int32_t)a0 + (int32_t)a1;
        int32_t diff = (int32_t)a0 - (int32_t)a1;
        r->a[x] = q_mul((cos_v91_q16_t)sum,  COS_V91_INV_SQRT2);
        r->a[y] = q_mul((cos_v91_q16_t)diff, COS_V91_INV_SQRT2);
    }
    ++r->ops;
}

/* CNOT: if control=1, toggle target. */
void cos_v91_cnot(cos_v91_reg_t *r, uint32_t c, uint32_t t)
{
    uint32_t mc = 1u << c;
    uint32_t mt = 1u << t;
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        if ((x & mc) == 0) continue;
        uint32_t y = x ^ mt;
        if (x < y) {
            cos_v91_q16_t tmp = r->a[x];
            r->a[x] = r->a[y];
            r->a[y] = tmp;
        }
    }
    ++r->ops;
}

void cos_v91_oracle(cos_v91_reg_t *r, uint32_t marked)
{
    marked &= (COS_V91_DIM - 1u);
    r->a[marked] = (cos_v91_q16_t)(-(int32_t)r->a[marked]);
    ++r->ops;
}

/* Grover diffusion 2|s><s| - I:  a'[x] = 2<s|ψ> * (1/√N) - a[x],
 *   where |s> = (1/√N) Σ_x |x>, <s|ψ> = (1/√N) Σ_x a[x].
 * So a'[x] = (2/N) Σ_y a[y] - a[x].
 *
 * With N = 16, 2/N = 1/8 → right-shift by 3. */
void cos_v91_diffusion(cos_v91_reg_t *r)
{
    int64_t s = 0;
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) s += (int64_t)r->a[x];
    int32_t mean2 = (int32_t)(s >> 3);          /* 2/N · Σ, N=16 */
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        r->a[x] = (cos_v91_q16_t)(mean2 - (int32_t)r->a[x]);
    }
    ++r->ops;
}

/* Canonical Grover on 4 qubits: apply H to all qubits, then 3
 * iterations of (oracle · diffusion). For N = 16 the optimum is
 * ⌊π/4 · √N⌋ = 3. */
void cos_v91_grover(cos_v91_reg_t *r, uint32_t marked)
{
    for (uint32_t q = 0; q < COS_V91_NQUBITS; ++q) cos_v91_h(r, q);
    for (uint32_t k = 0; k < 3u; ++k) {
        cos_v91_oracle(r, marked);
        cos_v91_diffusion(r);
    }
}

uint32_t cos_v91_argmax(const cos_v91_reg_t *r)
{
    uint32_t best = 0;
    int32_t  best_sq = 0;
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        int32_t v = (int32_t)r->a[x];
        int32_t sq = (int32_t)(((int64_t)v * (int64_t)v) >> 16);
        if (sq > best_sq) { best_sq = sq; best = x; }
    }
    return best;
}

/* Σ a_x^2 in Q32.32 (since each a_x^2 is Q32.32 before >>16). */
int64_t cos_v91_prob_mass_q32(const cos_v91_reg_t *r)
{
    int64_t s = 0;
    for (uint32_t x = 0; x < COS_V91_DIM; ++x) {
        int64_t v = (int64_t)r->a[x];
        s += v * v;                    /* Q32.32 */
    }
    return s;
}

uint32_t cos_v91_ok(const cos_v91_reg_t *r, uint32_t marked)
{
    uint32_t sent = (r->sentinel == COS_V91_SENTINEL) ? 1u : 0u;
    uint32_t viol = (r->invariant_violations == 0u)   ? 1u : 0u;
    /* Probability mass must be ≈ 1.0; in Q32.32, 1.0 = 2^32 ≈ 4.29e9.
     * Allow ±1/256 quantization slack. */
    int64_t  pm   = cos_v91_prob_mass_q32(r);
    int64_t  one  = (int64_t)1 << 32;
    int64_t  slack = one >> 8;              /* 1/256 of unity */
    int64_t  d    = pm - one;
    if (d < 0) d = -d;
    uint32_t pmok = (d < slack) ? 1u : 0u;
    uint32_t amok = (cos_v91_argmax(r) == (marked & (COS_V91_DIM - 1u))) ? 1u : 0u;
    return sent & viol & pmok & amok;
}

uint32_t cos_v91_compose_decision(uint32_t v90_composed_ok, uint32_t v91_ok)
{
    return v90_composed_ok & v91_ok;
}
