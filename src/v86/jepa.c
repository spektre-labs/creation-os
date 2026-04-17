/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v86 — σ-JEPA implementation.  Q16.16 integer math.
 * Branchless-ish hot path.  Deterministic by seed.
 */

#include "jepa.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 *  Helpers                                                           *
 * ------------------------------------------------------------------ */

static inline cos_v86_q16_t q_abs(cos_v86_q16_t x)
{
    uint32_t ux = (uint32_t)x;
    uint32_t m  = (uint32_t)(x >> 31);
    return (cos_v86_q16_t)((ux ^ m) - m);
}

static inline cos_v86_q16_t q_relu(cos_v86_q16_t x)
{
    /* Branchless: (x > 0) ? x : 0. */
    int32_t m = (int32_t)(((uint32_t)x) >> 31);   /* 1 if negative */
    return (cos_v86_q16_t)(((uint32_t)x) & (uint32_t)(m - 1));
}

/* Deterministic 64-bit xorshift. */
static inline uint64_t xs64(uint64_t *s)
{
    uint64_t x = *s; x ^= x << 13; x ^= x >> 7; x ^= x << 17; *s = x;
    return x;
}

/* Draw a small Q16.16 value in [-1, +1]. */
static inline cos_v86_q16_t draw_small(uint64_t *s)
{
    uint64_t r = xs64(s);
    /* 17 bits in range [-0x1_0000, +0x1_0000)  -> [-1.0, +1.0). */
    int32_t v = (int32_t)(r & 0x1FFFFu) - 0x10000;
    return (cos_v86_q16_t)v;
}

/* ------------------------------------------------------------------ *
 *  Init                                                              *
 * ------------------------------------------------------------------ */

void cos_v86_jepa_init(cos_v86_jepa_t *j, uint64_t seed)
{
    memset(j, 0, sizeof(*j));
    j->sentinel = COS_V86_SENTINEL;
    j->rollout_budget = (uint32_t)(2 << 16);    /* L1 ≤ 2.0 */
    j->variance_floor = (uint32_t)(1 << 12);    /* ≥ 1/16 */

    uint64_t s = seed ^ 0xA86A86A86A86A86Aull;
    for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
        for (uint32_t k = 0; k < COS_V86_OBS; ++k) {
            j->W_e[i][k] = draw_small(&s) >> 4;  /* /16 → Xavier-ish */
            j->W_t[i][k] = j->W_e[i][k];
        }
        j->b_e[i] = 0;
        j->b_t[i] = 0;

        for (uint32_t k = 0; k < COS_V86_DIM + COS_V86_ACT; ++k) {
            j->W_p[i][k] = draw_small(&s) >> 4;
        }
        j->b_p[i] = 0;
    }
}

/* ------------------------------------------------------------------ *
 *  Encoders                                                          *
 * ------------------------------------------------------------------ */

static void affine_relu(const cos_v86_q16_t W[COS_V86_DIM][COS_V86_OBS],
                        const cos_v86_q16_t b[COS_V86_DIM],
                        const cos_v86_q16_t x[COS_V86_OBS],
                        cos_v86_q16_t       z[COS_V86_DIM])
{
    for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
        int64_t acc = (int64_t)b[i] << 0;
        for (uint32_t k = 0; k < COS_V86_OBS; ++k) {
            acc += (int64_t)W[i][k] * (int64_t)x[k];
        }
        /* Q16.16 shift. */
        cos_v86_q16_t v = (cos_v86_q16_t)(acc >> 16);
        z[i] = q_relu(v);
    }
}

void cos_v86_encode(const cos_v86_jepa_t *j,
                    const cos_v86_q16_t x[COS_V86_OBS],
                    cos_v86_q16_t       z[COS_V86_DIM])
{
    affine_relu(j->W_e, j->b_e, x, z);
}

void cos_v86_target_encode(const cos_v86_jepa_t *j,
                           const cos_v86_q16_t x[COS_V86_OBS],
                           cos_v86_q16_t       z[COS_V86_DIM])
{
    affine_relu(j->W_t, j->b_t, x, z);
}

/* ------------------------------------------------------------------ *
 *  Predictor                                                         *
 * ------------------------------------------------------------------ */

void cos_v86_predict(const cos_v86_jepa_t *j,
                     const cos_v86_q16_t z[COS_V86_DIM],
                     const cos_v86_q16_t a[COS_V86_ACT],
                     cos_v86_q16_t       zhat[COS_V86_DIM])
{
    cos_v86_q16_t za[COS_V86_DIM + COS_V86_ACT];
    for (uint32_t i = 0; i < COS_V86_DIM; ++i) za[i] = z[i];
    for (uint32_t i = 0; i < COS_V86_ACT; ++i) za[COS_V86_DIM + i] = a[i];

    for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
        int64_t acc = (int64_t)j->b_p[i];
        for (uint32_t k = 0; k < COS_V86_DIM + COS_V86_ACT; ++k) {
            acc += (int64_t)j->W_p[i][k] * (int64_t)za[k];
        }
        zhat[i] = (cos_v86_q16_t)(acc >> 16);
    }
}

/* ------------------------------------------------------------------ *
 *  Losses                                                            *
 * ------------------------------------------------------------------ */

cos_v86_q16_t cos_v86_pred_loss(const cos_v86_q16_t zhat[COS_V86_DIM],
                                const cos_v86_q16_t ztgt[COS_V86_DIM])
{
    uint32_t acc = 0;
    for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
        uint32_t d = (uint32_t)q_abs((cos_v86_q16_t)
                       ((uint32_t)zhat[i] - (uint32_t)ztgt[i]));
        acc += d;
    }
    /* Return the *mean* absolute error (L1 / D). */
    return (cos_v86_q16_t)(acc / COS_V86_DIM);
}

cos_v86_q16_t cos_v86_vicreg_invar(const cos_v86_q16_t z [COS_V86_DIM],
                                   const cos_v86_q16_t zp[COS_V86_DIM])
{
    return cos_v86_pred_loss(z, zp);
}

cos_v86_q16_t cos_v86_vicreg_var(const cos_v86_q16_t *Z, uint32_t N)
{
    if (N < 2u) return 0;
    cos_v86_q16_t mins = INT32_MAX;
    for (uint32_t d = 0; d < COS_V86_DIM; ++d) {
        int64_t mean = 0;
        for (uint32_t n = 0; n < N; ++n) mean += (int64_t)Z[n * COS_V86_DIM + d];
        mean /= (int64_t)N;
        int64_t var = 0;
        for (uint32_t n = 0; n < N; ++n) {
            int64_t dd = (int64_t)Z[n * COS_V86_DIM + d] - mean;
            var += (dd * dd) >> 16;              /* Q16.16 square */
        }
        cos_v86_q16_t v = (cos_v86_q16_t)(var / (int64_t)N);
        if (v < mins) mins = v;
    }
    return mins;
}

cos_v86_q16_t cos_v86_vicreg_covar(const cos_v86_q16_t *Z, uint32_t N)
{
    if (N < 2u) return 0;
    cos_v86_q16_t mean[COS_V86_DIM];
    for (uint32_t d = 0; d < COS_V86_DIM; ++d) {
        int64_t m = 0;
        for (uint32_t n = 0; n < N; ++n) m += (int64_t)Z[n * COS_V86_DIM + d];
        mean[d] = (cos_v86_q16_t)(m / (int64_t)N);
    }
    uint64_t acc = 0;
    for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
        for (uint32_t j = i + 1u; j < COS_V86_DIM; ++j) {
            int64_t c = 0;
            for (uint32_t n = 0; n < N; ++n) {
                int64_t di = (int64_t)Z[n * COS_V86_DIM + i] - (int64_t)mean[i];
                int64_t dj = (int64_t)Z[n * COS_V86_DIM + j] - (int64_t)mean[j];
                c += (di * dj) >> 16;
            }
            int64_t v = c / (int64_t)N;
            acc += (uint64_t)q_abs((cos_v86_q16_t)v);
        }
    }
    /* Return summed off-diagonal abs covariance. */
    return (cos_v86_q16_t)(acc > 0x7FFFFFFFu ? 0x7FFFFFFF : acc);
}

/* ------------------------------------------------------------------ *
 *  EMA target update                                                 *
 * ------------------------------------------------------------------ */

void cos_v86_ema_update(cos_v86_jepa_t *j)
{
    /* τ = 1/8 as a right-shift; W_t ← W_t - (W_t - W_e) >> 3. */
    for (uint32_t i = 0; i < COS_V86_DIM; ++i) {
        for (uint32_t k = 0; k < COS_V86_OBS; ++k) {
            int32_t diff = (int32_t)(((uint32_t)j->W_t[i][k]) -
                                     ((uint32_t)j->W_e[i][k]));
            /* Arithmetic shift right 3 (keeps sign). */
            int32_t step = diff / 8;
            j->W_t[i][k] = (cos_v86_q16_t)
                           ((uint32_t)j->W_t[i][k] - (uint32_t)step);
        }
        int32_t db = (int32_t)(((uint32_t)j->b_t[i]) - ((uint32_t)j->b_e[i]));
        int32_t sb = db / 8;
        j->b_t[i] = (cos_v86_q16_t)((uint32_t)j->b_t[i] - (uint32_t)sb);
    }
}

/* ------------------------------------------------------------------ *
 *  One rollout step                                                  *
 * ------------------------------------------------------------------ */

cos_v86_q16_t cos_v86_rollout_step(cos_v86_jepa_t *j,
                                   const cos_v86_q16_t x_t  [COS_V86_OBS],
                                   const cos_v86_q16_t a_t  [COS_V86_ACT],
                                   const cos_v86_q16_t x_tp1[COS_V86_OBS])
{
    if (j->sentinel != COS_V86_SENTINEL) {
        ++j->invariant_violations;
        return INT32_MAX;
    }

    cos_v86_q16_t z[COS_V86_DIM], zhat[COS_V86_DIM], ztgt[COS_V86_DIM];
    cos_v86_encode(j, x_t, z);
    cos_v86_predict(j, z, a_t, zhat);
    cos_v86_target_encode(j, x_tp1, ztgt);

    cos_v86_q16_t err = cos_v86_pred_loss(zhat, ztgt);
    j->last_pred_err = err;
    ++j->steps;
    return err;
}

/* ------------------------------------------------------------------ *
 *  Gate + compose                                                    *
 * ------------------------------------------------------------------ */

uint32_t cos_v86_ok(const cos_v86_jepa_t *j)
{
    uint32_t sent   = (j->sentinel == COS_V86_SENTINEL) ? 1u : 0u;
    uint32_t noviol = (j->invariant_violations == 0u)   ? 1u : 0u;
    uint32_t err_ok = ((uint32_t)j->last_pred_err <= j->rollout_budget) ? 1u : 0u;
    return sent & noviol & err_ok;
}

uint32_t cos_v86_compose_decision(uint32_t v85_composed_ok, uint32_t v86_ok)
{
    return v85_composed_ok & v86_ok;
}
