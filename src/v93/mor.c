/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v93 — σ-MoR implementation.
 */

#include "mor.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint64_t xs(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

static inline cos_v93_q16_t q_mul(cos_v93_q16_t a, cos_v93_q16_t b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    return (cos_v93_q16_t)(p >> 16);
}

static inline cos_v93_q16_t q_clamp(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (cos_v93_q16_t)v;
}

void cos_v93_mor_init(cos_v93_mor_t *m, uint64_t seed, uint32_t n_tokens)
{
    memset(m, 0, sizeof(*m));
    m->sentinel = COS_V93_SENTINEL;
    if (n_tokens > COS_V93_MAX_TOKENS) n_tokens = COS_V93_MAX_TOKENS;
    m->n_tokens = n_tokens;

    uint64_t s = seed ? seed : 0x93F40017ULL;

    /* W: small random weights (±1/16) so that || W·h || stays bounded
     * well within the clamp when applied to |h| ≤ CLAMP. */
    for (uint32_t i = 0; i < COS_V93_HDIM; ++i) {
        for (uint32_t j = 0; j < COS_V93_HDIM; ++j) {
            int32_t r = (int32_t)(xs(&s) & 0x1FFFu) - 0x1000;    /* ±0.0625 */
            m->W[i][j] = (cos_v93_q16_t)r;
        }
        m->b[i]  = (cos_v93_q16_t)((int32_t)(xs(&s) & 0xFFFu) - 0x800);
        m->rw[i] = (cos_v93_q16_t)((int32_t)(xs(&s) & 0x7FFFu) - 0x4000);
    }

    /* Thresholds grow with depth so later steps exit only for truly
     * certain tokens (harder tokens keep recursing). */
    for (uint32_t r = 0; r < COS_V93_MAX_DEPTH; ++r) {
        int32_t t = (int32_t)(1u << 15) + (int32_t)r * (int32_t)(1u << 13);
        m->thresh[r] = (cos_v93_q16_t)t;
    }
}

void cos_v93_mor_set_input(cos_v93_mor_t *m, uint32_t t,
                           const cos_v93_q16_t h0[COS_V93_HDIM])
{
    if (t >= m->n_tokens) return;
    for (uint32_t d = 0; d < COS_V93_HDIM; ++d) {
        m->h[t][d] = q_clamp(h0[d], -COS_V93_HCLAMP, COS_V93_HCLAMP);
    }
    m->exit_d[t] = COS_V93_MAX_DEPTH;              /* not exited yet */
    m->active[t] = 1u;
}

static void apply_R(const cos_v93_mor_t *m,
                    const cos_v93_q16_t in [COS_V93_HDIM],
                    cos_v93_q16_t       out[COS_V93_HDIM])
{
    /* out = clamp(in + W·in + b) */
    for (uint32_t i = 0; i < COS_V93_HDIM; ++i) {
        int64_t acc = (int64_t)in[i] + (int64_t)m->b[i];
        for (uint32_t j = 0; j < COS_V93_HDIM; ++j) {
            acc += (int64_t)q_mul(m->W[i][j], in[j]);
        }
        out[i] = q_clamp((int32_t)acc, -COS_V93_HCLAMP, COS_V93_HCLAMP);
    }
}

static cos_v93_q16_t router_score(const cos_v93_mor_t *m,
                                  const cos_v93_q16_t  h[COS_V93_HDIM])
{
    int64_t s = 0;
    for (uint32_t d = 0; d < COS_V93_HDIM; ++d) {
        s += (int64_t)q_mul(m->rw[d], h[d]);
    }
    return (cos_v93_q16_t)s;
}

void cos_v93_mor_run(cos_v93_mor_t *m)
{
    for (uint32_t r = 0; r < COS_V93_MAX_DEPTH; ++r) {
        for (uint32_t t = 0; t < m->n_tokens; ++t) {
            if (!m->active[t]) continue;
            cos_v93_q16_t out[COS_V93_HDIM];
            apply_R(m, m->h[t], out);
            memcpy(m->h[t], out, sizeof(out));
            cos_v93_q16_t score = router_score(m, m->h[t]);
            if (score > m->thresh[r]) {
                m->exit_d[t] = r + 1u;        /* exit after this step */
                m->active[t] = 0u;
            } else if (r == COS_V93_MAX_DEPTH - 1u) {
                m->exit_d[t] = COS_V93_MAX_DEPTH;
                m->active[t] = 0u;
            }
        }
        ++m->steps_run;
    }
}

cos_v93_q16_t cos_v93_mor_avg_depth(const cos_v93_mor_t *m)
{
    if (m->n_tokens == 0u) return 0;
    uint32_t sum = 0;
    for (uint32_t t = 0; t < m->n_tokens; ++t) sum += m->exit_d[t];
    /* Q16.16 = (sum << 16) / n */
    return (cos_v93_q16_t)(((uint64_t)sum << 16) / m->n_tokens);
}

uint32_t cos_v93_ok(const cos_v93_mor_t *m)
{
    uint32_t sent = (m->sentinel == COS_V93_SENTINEL) ? 1u : 0u;
    uint32_t h_ok = 1u;
    for (uint32_t t = 0; t < m->n_tokens; ++t) {
        for (uint32_t d = 0; d < COS_V93_HDIM; ++d) {
            int32_t v = m->h[t][d];
            if (v < -COS_V93_HCLAMP || v > COS_V93_HCLAMP) h_ok = 0u;
        }
        if (m->exit_d[t] > COS_V93_MAX_DEPTH) h_ok = 0u;
    }
    uint32_t depth_ok = ((uint32_t)cos_v93_mor_avg_depth(m)
                         <= (uint32_t)(COS_V93_MAX_DEPTH << 16)) ? 1u : 0u;
    return sent & h_ok & depth_ok;
}

uint32_t cos_v93_compose_decision(uint32_t v92_composed_ok, uint32_t v93_ok)
{
    return v92_composed_ok & v93_ok;
}
