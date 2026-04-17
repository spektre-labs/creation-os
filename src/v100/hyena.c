/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v100 — σ-Hyena implementation.
 */

#include "hyena.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline cos_v100_q16_t q_mul(cos_v100_q16_t a, cos_v100_q16_t b)
{
    return (cos_v100_q16_t)(((int64_t)a * (int64_t)b) >> 16);
}

static inline cos_v100_q16_t q_clamp(cos_v100_q16_t x,
                                     cos_v100_q16_t lo,
                                     cos_v100_q16_t hi)
{
    cos_v100_q16_t y = (x < lo) ? lo : x;
    y               = (y > hi) ? hi : y;
    return y;
}

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

void cos_v100_init(cos_v100_hyena_t *h, uint64_t seed)
{
    memset(h, 0, sizeof(*h));
    h->sentinel = COS_V100_SENTINEL;
    uint64_t r = seed ? seed : 0xA0C0FEE0A0C0FEE0ULL;

    /* Filter h_k = sign_k · γ^k, accumulated multiplicatively in Q16.16.
     * Because γ<1, |h_k| is monotone non-increasing in k.             */
    cos_v100_q16_t accum = COS_V100_Q1;
    for (uint32_t k = 0u; k < COS_V100_L; ++k) {
        int32_t sign = (xs(&r) & 1u) ? 1 : -1;
        h->h[k] = (cos_v100_q16_t)(sign * (int32_t)accum);
        accum   = q_mul(accum, (cos_v100_q16_t)COS_V100_GAMMA);
    }

    /* Gate g_t ∈ [0, Q1].  Generate deterministic integer values then
     * clamp to the valid range. */
    for (uint32_t t = 0u; t < COS_V100_L; ++t) {
        h->g[t] = (cos_v100_q16_t)(xs(&r) & 0xFFFFu);   /* [0, Q1-1] */
    }
}

void cos_v100_conv_only(const cos_v100_hyena_t *h,
                        const cos_v100_q16_t x[COS_V100_L],
                        cos_v100_q16_t       y[COS_V100_L])
{
    for (uint32_t t = 0u; t < COS_V100_L; ++t) {
        int64_t acc = 0;
        for (uint32_t k = 0u; k <= t; ++k) {
            int64_t term = (int64_t)h->h[k] * (int64_t)x[t - k];
            acc += term;
        }
        /* Shift once at the end (Q32.32 → Q16.16). */
        int64_t v = acc >> 16;
        if (v >  (int64_t)COS_V100_CLAMP) v =  (int64_t)COS_V100_CLAMP;
        if (v < -(int64_t)COS_V100_CLAMP) v = -(int64_t)COS_V100_CLAMP;
        y[t] = (cos_v100_q16_t)v;
    }
}

void cos_v100_apply(cos_v100_hyena_t *h,
                    const cos_v100_q16_t x[COS_V100_L],
                    cos_v100_q16_t       y[COS_V100_L])
{
    cos_v100_q16_t pre[COS_V100_L];
    cos_v100_conv_only(h, x, pre);
    for (uint32_t t = 0u; t < COS_V100_L; ++t) {
        cos_v100_q16_t gated = q_mul(h->g[t], pre[t]);
        cos_v100_q16_t out   = q_clamp(gated,
                                       (cos_v100_q16_t)(-COS_V100_CLAMP),
                                       (cos_v100_q16_t)( COS_V100_CLAMP));
        if (out == (cos_v100_q16_t)COS_V100_CLAMP ||
            out == (cos_v100_q16_t)(-COS_V100_CLAMP)) {
            ++h->clamp_violations;
        }
        y[t] = out;
    }
}

uint32_t cos_v100_ok(const cos_v100_hyena_t *h)
{
    uint32_t sent = (h->sentinel == COS_V100_SENTINEL) ? 1u : 0u;
    /* Filter envelope monotone non-increasing in magnitude. */
    uint32_t mono = 1u;
    for (uint32_t k = 1u; k < COS_V100_L; ++k) {
        int32_t a = h->h[k - 1u];
        int32_t b = h->h[k];
        a = a < 0 ? -a : a;
        b = b < 0 ? -b : b;
        if (b > a) mono = 0u;
    }
    /* Gate bounded. */
    uint32_t gok = 1u;
    for (uint32_t t = 0u; t < COS_V100_L; ++t) {
        if (h->g[t] < 0)               gok = 0u;
        if (h->g[t] > (cos_v100_q16_t)COS_V100_Q1) gok = 0u;
    }
    return sent & mono & gok;
}

uint32_t cos_v100_compose_decision(uint32_t v99_composed_ok, uint32_t v100_ok)
{
    return v99_composed_ok & v100_ok;
}
