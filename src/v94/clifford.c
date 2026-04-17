/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v94 — σ-Clifford implementation (Cl(3,0)).
 */

#include "clifford.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define COS_V94_CLAMP  (4 << 16)

static inline cos_v94_q16_t q_clamp(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (cos_v94_q16_t)v;
}

/* Count the number of pair-swaps needed to merge basis multivectors
 * e_A (bitset A) and e_B (bitset B).  For Cl(n,0), e_i²=+1 so the
 * result basis is simply A XOR B, and the sign is (-1)^{swaps}, where
 *
 *     swaps = Σ_{j ∈ B} popcount(A & upper_mask(j))
 *
 * with upper_mask(j) = bits strictly above j.
 */
static uint32_t count_swaps(uint32_t A, uint32_t B)
{
    uint32_t sw = 0;
    for (uint32_t j = 0; j < 3u; ++j) {
        if (!((B >> j) & 1u)) continue;
        uint32_t upper = A & (~((1u << (j + 1u)) - 1u)) & 0x7u;
        sw += (uint32_t)__builtin_popcount(upper);
    }
    return sw;
}

void cos_v94_alg_init(cos_v94_algebra_t *a)
{
    memset(a, 0, sizeof(*a));
    a->sentinel = COS_V94_SENTINEL;
    for (uint32_t A = 0; A < COS_V94_DIM; ++A) {
        for (uint32_t B = 0; B < COS_V94_DIM; ++B) {
            uint32_t idx = (A ^ B) & 0x7u;
            uint32_t sw  = count_swaps(A, B);
            a->prod_idx [A][B] = (uint8_t)idx;
            a->prod_sign[A][B] = (sw & 1u) ? -1 : +1;
        }
    }
}

void cos_v94_gp(const cos_v94_algebra_t *a,
                const cos_v94_mv_t u, const cos_v94_mv_t v,
                cos_v94_mv_t out)
{
    /* Accumulate products in Q32.32 (no per-term rounding) and shift
     * once at the end, so the geometric product is bit-exact and
     * associative up to a single final quantization. */
    int64_t acc[COS_V94_DIM] = {0};
    for (uint32_t A = 0; A < COS_V94_DIM; ++A) {
        for (uint32_t B = 0; B < COS_V94_DIM; ++B) {
            int64_t p = (int64_t)u[A] * (int64_t)v[B];   /* Q32.32 */
            if (a->prod_sign[A][B] < 0) p = -p;
            acc[a->prod_idx[A][B]] += p;
        }
    }
    for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
        out[i] = (cos_v94_q16_t)(acc[i] >> 16);
    }
}

/* Grade of basis index i = popcount(i). Reverse sign = (-1)^{k(k-1)/2}. */
static int8_t rev_sign(uint32_t i)
{
    uint32_t k = (uint32_t)__builtin_popcount(i);
    uint32_t s = (k * (k - 1u)) / 2u;
    return (s & 1u) ? -1 : +1;
}

void cos_v94_rev(const cos_v94_mv_t u, cos_v94_mv_t out)
{
    for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
        out[i] = (rev_sign(i) > 0) ? u[i] : (cos_v94_q16_t)(-(int32_t)u[i]);
    }
}

void cos_v94_grade(const cos_v94_mv_t u, uint32_t k, cos_v94_mv_t out)
{
    for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
        out[i] = ((uint32_t)__builtin_popcount(i) == k) ? u[i] : 0;
    }
}

void cos_v94_inner(const cos_v94_algebra_t *a,
                   const cos_v94_mv_t u, const cos_v94_mv_t v,
                   cos_v94_mv_t out)
{
    cos_v94_mv_t uv, vu;
    cos_v94_gp(a, u, v, uv);
    cos_v94_gp(a, v, u, vu);
    for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
        out[i] = (cos_v94_q16_t)(((int32_t)uv[i] + (int32_t)vu[i]) / 2);
    }
}

void cos_v94_wedge(const cos_v94_algebra_t *a,
                   const cos_v94_mv_t u, const cos_v94_mv_t v,
                   cos_v94_mv_t out)
{
    cos_v94_mv_t uv, vu;
    cos_v94_gp(a, u, v, uv);
    cos_v94_gp(a, v, u, vu);
    for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
        out[i] = (cos_v94_q16_t)(((int32_t)uv[i] - (int32_t)vu[i]) / 2);
    }
}

void cos_v94_layer(const cos_v94_algebra_t *a,
                   const cos_v94_mv_t u, const cos_v94_mv_t v,
                   const cos_v94_mv_t bias,
                   cos_v94_mv_t y)
{
    cos_v94_mv_t uv;
    cos_v94_gp(a, u, v, uv);
    for (uint32_t i = 0; i < COS_V94_DIM; ++i) {
        int32_t s = (int32_t)uv[i] + (int32_t)bias[i];
        y[i] = q_clamp(s, -COS_V94_CLAMP, COS_V94_CLAMP);
    }
}

uint32_t cos_v94_ok(const cos_v94_algebra_t *a)
{
    uint32_t sent = (a->sentinel == COS_V94_SENTINEL) ? 1u : 0u;
    uint32_t ok = 1u;
    for (uint32_t A = 0; A < COS_V94_DIM; ++A) {
        for (uint32_t B = 0; B < COS_V94_DIM; ++B) {
            if (a->prod_idx[A][B] >= COS_V94_DIM) ok = 0u;
            int s = a->prod_sign[A][B];
            if (s != 1 && s != -1) ok = 0u;
        }
        /* e_A · 1 = e_A, and 1 · e_A = e_A with sign +1 and index A. */
        if (a->prod_idx[A][0] != A) ok = 0u;
        if (a->prod_sign[A][0] != 1) ok = 0u;
        if (a->prod_idx[0][A] != A) ok = 0u;
        if (a->prod_sign[0][A] != 1) ok = 0u;
    }
    return sent & ok;
}

uint32_t cos_v94_compose_decision(uint32_t v93_composed_ok, uint32_t v94_ok)
{
    return v93_composed_ok & v94_ok;
}
