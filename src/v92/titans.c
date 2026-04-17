/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v92 — σ-Titans implementation.
 */

#include "titans.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

static inline cos_v92_q16_t q_abs(cos_v92_q16_t x)
{
    uint32_t ux = (uint32_t)x;
    uint32_t m  = (uint32_t)(x >> 31);
    return (cos_v92_q16_t)((ux ^ m) - m);
}

static inline cos_v92_q16_t q_clamp(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (cos_v92_q16_t)v;
}

void cos_v92_mem_init(cos_v92_mem_t *m, uint64_t seed)
{
    memset(m, 0, sizeof(*m));
    m->sentinel = COS_V92_SENTINEL;
    uint64_t s = seed ? seed : 0x9217AA5E9217AA5EULL;
    /* Seed keys to a small pseudo-random Q16.16 lattice (|k| < 0.5).
     * Values initialized to zero — the memory "learns" from events. */
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) {
        for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d) {
            int32_t r = (int32_t)(xs(&s) & 0x7FFFu) - 0x4000;   /* ±0.25 */
            m->keys[i][d] = (cos_v92_q16_t)r;
        }
    }
}

static int64_t dot16(const cos_v92_q16_t a[COS_V92_KEY_DIM],
                     const cos_v92_q16_t b[COS_V92_KEY_DIM])
{
    int64_t s = 0;
    for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d) {
        s += (int64_t)a[d] * (int64_t)b[d];       /* Q32.32 */
    }
    return s;
}

void cos_v92_retrieve(const cos_v92_mem_t *m,
                      const cos_v92_q16_t  key[COS_V92_KEY_DIM],
                      cos_v92_q16_t        yhat[COS_V92_VAL_DIM],
                      uint32_t            *best)
{
    int64_t  best_s = INT64_MIN;
    uint32_t best_i = 0;
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) {
        int64_t s = dot16(key, m->keys[i]);
        if (s > best_s) { best_s = s; best_i = i; }
    }
    for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d) yhat[d] = m->vals[best_i][d];
    if (best) *best = best_i;
}

cos_v92_q16_t cos_v92_event(cos_v92_mem_t       *m,
                            const cos_v92_q16_t  key   [COS_V92_KEY_DIM],
                            const cos_v92_q16_t  target[COS_V92_VAL_DIM])
{
    ++m->events;

    /* 1. retrieve */
    cos_v92_q16_t yhat[COS_V92_VAL_DIM];
    uint32_t best = 0;
    cos_v92_retrieve(m, key, yhat, &best);

    /* 2. surprise = L1 error */
    uint32_t acc = 0;
    for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d) {
        acc += (uint32_t)q_abs((cos_v92_q16_t)((int32_t)target[d] - (int32_t)yhat[d]));
    }
    cos_v92_q16_t S = (cos_v92_q16_t)(acc / COS_V92_VAL_DIM);

    /* 3. write gate */
    if ((uint32_t)S > (uint32_t)COS_V92_SURPRISE_LO) {
        /* overwrite the slot with smallest usage */
        uint32_t victim = 0;
        uint32_t u_min  = UINT32_MAX;
        for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) {
            if (m->usage[i] < u_min) { u_min = m->usage[i]; victim = i; }
        }
        for (uint32_t d = 0; d < COS_V92_KEY_DIM; ++d) {
            m->keys[victim][d] = key[d];
        }
        for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d) {
            /* momentum: v_new = target + β·v_best where β = 1/4 via rshift */
            int32_t blended = (int32_t)target[d]
                            + ((int32_t)m->vals[best][d] >> COS_V92_MOMENTUM_RSHIFT);
            m->vals[victim][d] = q_clamp(blended, -COS_V92_VAL_CLAMP, COS_V92_VAL_CLAMP);
        }
        m->usage[victim] = 1u;
        ++m->writes;
    } else {
        /* reinforce retrieved slot */
        if (m->usage[best] < COS_V92_USAGE_MAX) ++m->usage[best];
    }

    if ((m->events % COS_V92_FORGET_PERIOD) == 0u) cos_v92_forget(m);
    return S;
}

void cos_v92_forget(cos_v92_mem_t *m)
{
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) {
        /* u ← u · 31/32 */
        m->usage[i] = (m->usage[i] * 31u) >> 5;
    }
}

uint32_t cos_v92_ok(const cos_v92_mem_t *m)
{
    uint32_t sent = (m->sentinel == COS_V92_SENTINEL) ? 1u : 0u;
    uint32_t u_ok = 1u;
    uint32_t v_ok = 1u;
    for (uint32_t i = 0; i < COS_V92_NUM_SLOTS; ++i) {
        if (m->usage[i] > COS_V92_USAGE_MAX) u_ok = 0u;
        for (uint32_t d = 0; d < COS_V92_VAL_DIM; ++d) {
            int32_t v = m->vals[i][d];
            if (v < -COS_V92_VAL_CLAMP || v > COS_V92_VAL_CLAMP) v_ok = 0u;
        }
    }
    return sent & u_ok & v_ok;
}

uint32_t cos_v92_compose_decision(uint32_t v91_composed_ok, uint32_t v92_ok)
{
    return v91_composed_ok & v92_ok;
}
