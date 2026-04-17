/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v87 — σ-SAE implementation.  Q16.16 integer math.
 * Top-K rule is exact (|{i : a_i != 0}| == K strictly).
 */

#include "sae.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 *  Helpers                                                           *
 * ------------------------------------------------------------------ */

static inline cos_v87_q16_t q_abs(cos_v87_q16_t x)
{
    uint32_t ux = (uint32_t)x;
    uint32_t m  = (uint32_t)(x >> 31);
    return (cos_v87_q16_t)((ux ^ m) - m);
}

static inline cos_v87_q16_t q_relu(cos_v87_q16_t x)
{
    int32_t m = (int32_t)(((uint32_t)x) >> 31);
    return (cos_v87_q16_t)(((uint32_t)x) & (uint32_t)(m - 1));
}

static inline uint64_t xs64(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

static inline cos_v87_q16_t draw_small(uint64_t *s)
{
    uint64_t r = xs64(s);
    int32_t v = (int32_t)(r & 0x1FFFFu) - 0x10000;
    return (cos_v87_q16_t)v;
}

/* ------------------------------------------------------------------ *
 *  Init                                                              *
 * ------------------------------------------------------------------ */

void cos_v87_sae_init(cos_v87_sae_t *s, uint64_t seed)
{
    memset(s, 0, sizeof(*s));
    s->sentinel = COS_V87_SENTINEL;
    s->recon_budget = (uint32_t)(3 << 16);    /* L1 avg ≤ 3.0 */

    uint64_t r = seed ^ 0x5A47E5A47E5A47EAull;
    for (uint32_t f = 0; f < COS_V87_FEAT; ++f) {
        for (uint32_t d = 0; d < COS_V87_DIM; ++d) {
            s->W_enc[f][d] = draw_small(&r) >> 3;     /* /8 scale */
        }
        s->b_enc[f] = 0;
    }
    for (uint32_t d = 0; d < COS_V87_DIM; ++d) {
        for (uint32_t f = 0; f < COS_V87_FEAT; ++f) {
            s->W_dec[d][f] = draw_small(&r) >> 3;
        }
        s->b_dec[d] = 0;
    }
}

/* ------------------------------------------------------------------ *
 *  Top-K (exact) selection                                           *
 * ------------------------------------------------------------------ */

static void topk_select(const cos_v87_q16_t a[COS_V87_FEAT],
                        uint32_t out_ids[COS_V87_TOPK])
{
    /* O(F*K); F=64, K=4 -> 256 compares.  Stable: tie-break by id
     * (smaller id wins) so results are deterministic. */
    uint32_t taken[COS_V87_FEAT] = {0};
    for (uint32_t k = 0; k < COS_V87_TOPK; ++k) {
        int32_t best = INT32_MIN;
        uint32_t best_id = 0;
        uint32_t found = 0;
        for (uint32_t f = 0; f < COS_V87_FEAT; ++f) {
            if (taken[f]) continue;
            int32_t v = q_abs(a[f]);
            if (!found || v > best || (v == best && f < best_id)) {
                best = v;
                best_id = f;
                found = 1;
            }
        }
        out_ids[k] = best_id;
        taken[best_id] = 1;
    }
}

/* ------------------------------------------------------------------ *
 *  Encode                                                            *
 * ------------------------------------------------------------------ */

void cos_v87_encode(cos_v87_sae_t *s,
                    const cos_v87_q16_t h[COS_V87_DIM],
                    cos_v87_q16_t        a[COS_V87_FEAT],
                    uint32_t             topk_ids[COS_V87_TOPK])
{
    /* Dense activations: a_f = relu(W_enc_f · h + b_enc_f). */
    cos_v87_q16_t dense[COS_V87_FEAT];
    for (uint32_t f = 0; f < COS_V87_FEAT; ++f) {
        int64_t acc = (int64_t)s->b_enc[f];
        for (uint32_t d = 0; d < COS_V87_DIM; ++d) {
            acc += (int64_t)s->W_enc[f][d] * (int64_t)h[d];
        }
        dense[f] = q_relu((cos_v87_q16_t)(acc >> 16));
    }

    /* Top-K selection on the *relu* outputs. */
    topk_select(dense, topk_ids);

    /* Mask: zero everything outside the top-K. */
    memset(a, 0, sizeof(cos_v87_q16_t) * COS_V87_FEAT);
    for (uint32_t k = 0; k < COS_V87_TOPK; ++k) {
        a[topk_ids[k]] = dense[topk_ids[k]];
    }
    s->last_l0 = COS_V87_TOPK;
}

/* ------------------------------------------------------------------ *
 *  Decode                                                            *
 * ------------------------------------------------------------------ */

void cos_v87_decode(const cos_v87_sae_t *s,
                    const cos_v87_q16_t a [COS_V87_FEAT],
                    cos_v87_q16_t        hh[COS_V87_DIM])
{
    for (uint32_t d = 0; d < COS_V87_DIM; ++d) {
        int64_t acc = (int64_t)s->b_dec[d];
        for (uint32_t f = 0; f < COS_V87_FEAT; ++f) {
            acc += (int64_t)s->W_dec[d][f] * (int64_t)a[f];
        }
        hh[d] = (cos_v87_q16_t)(acc >> 16);
    }
}

/* ------------------------------------------------------------------ *
 *  Losses                                                            *
 * ------------------------------------------------------------------ */

cos_v87_q16_t cos_v87_recon_error(const cos_v87_q16_t hh[COS_V87_DIM],
                                  const cos_v87_q16_t h [COS_V87_DIM])
{
    uint32_t acc = 0;
    for (uint32_t d = 0; d < COS_V87_DIM; ++d) {
        uint32_t diff = (uint32_t)q_abs((cos_v87_q16_t)
                           ((uint32_t)hh[d] - (uint32_t)h[d]));
        acc += diff;
    }
    return (cos_v87_q16_t)(acc / COS_V87_DIM);
}

/* ------------------------------------------------------------------ *
 *  Attribution                                                       *
 * ------------------------------------------------------------------ */

void cos_v87_attribute(cos_v87_sae_t *s,
                       const cos_v87_q16_t h[COS_V87_DIM],
                       uint32_t            topk_ids[COS_V87_TOPK])
{
    cos_v87_q16_t a[COS_V87_FEAT];
    cos_v87_encode(s, h, a, topk_ids);
}

/* ------------------------------------------------------------------ *
 *  Ablation: causal feature tracing                                  *
 * ------------------------------------------------------------------ */

cos_v87_q16_t cos_v87_ablate(cos_v87_sae_t *s,
                             const cos_v87_q16_t h[COS_V87_DIM],
                             uint32_t fid)
{
    if (s->sentinel != COS_V87_SENTINEL) {
        ++s->invariant_violations;
        return INT32_MAX;
    }
    if (fid >= COS_V87_FEAT) {
        ++s->invariant_violations;
        return INT32_MAX;
    }

    cos_v87_q16_t a[COS_V87_FEAT];
    uint32_t ids[COS_V87_TOPK];
    cos_v87_encode(s, h, a, ids);

    cos_v87_q16_t hh_full[COS_V87_DIM];
    cos_v87_decode(s, a, hh_full);

    /* Ablate: if fid is in the top-K, zero it; otherwise the result
     * is identical (ablation of an inactive feature is a no-op). */
    a[fid] = 0;

    cos_v87_q16_t hh_abl[COS_V87_DIM];
    cos_v87_decode(s, a, hh_abl);

    cos_v87_q16_t delta = cos_v87_recon_error(hh_abl, hh_full);
    /* Record the baseline recon for gate purposes. */
    s->last_recon_err = cos_v87_recon_error(hh_full, h);
    return delta;
}

/* ------------------------------------------------------------------ *
 *  Gate + compose                                                    *
 * ------------------------------------------------------------------ */

uint32_t cos_v87_ok(const cos_v87_sae_t *s)
{
    uint32_t sent    = (s->sentinel == COS_V87_SENTINEL) ? 1u : 0u;
    uint32_t noviol  = (s->invariant_violations == 0u)   ? 1u : 0u;
    uint32_t l0_ok   = (s->last_l0 == COS_V87_TOPK)      ? 1u : 0u;
    uint32_t recon_ok= ((uint32_t)s->last_recon_err <= s->recon_budget) ? 1u : 0u;
    return sent & noviol & l0_ok & recon_ok;
}

uint32_t cos_v87_compose_decision(uint32_t v86_composed_ok, uint32_t v87_ok)
{
    return v86_composed_ok & v87_ok;
}
