/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Q16.16 σ state — algebraic mirror of `python/cos/sigma_gate_core.py`.
 * Do not edit `python/cos/sigma_gate.h` here; that file remains the
 * packaging hook for Python. This header is the C reference for inference.
 */
#ifndef COS_SIGMA_MIRROR_H
#define COS_SIGMA_MIRROR_H

#include <stdint.h>

#define COS_SIGMA_Q16        ((int32_t)65536)
#define COS_SIGMA_Q16_ONE    COS_SIGMA_Q16
#define COS_SIGMA_K_CRIT     ((int32_t)8323) /* round(0.127 * 65536) */

typedef struct {
    int32_t sigma;
    int32_t d_sigma;
    int32_t k_eff;
} cos_sigma_q16_state_t;

/* Map a normalized scalar in [0,1] to Q16 (clamped). */
static inline int32_t cos_sigma_q16_from_unit_q16(int32_t num, int32_t den)
{
    int64_t n;
    if (den <= 0)
        return 0;
    n = ((int64_t)num * COS_SIGMA_Q16_ONE) / (int64_t)den;
    if (n < 0)
        n = 0;
    if (n > COS_SIGMA_Q16_ONE - 1)
        n = COS_SIGMA_Q16_ONE - 1;
    return (int32_t)n;
}

static inline void cos_sigma_q16_init(cos_sigma_q16_state_t *st)
{
    if (!st)
        return;
    st->sigma    = 0;
    st->d_sigma  = 0;
    st->k_eff    = COS_SIGMA_Q16_ONE;
}

/* Matches `sigma_update_q16` in sigma_gate_core.py */
static inline void cos_sigma_q16_update(cos_sigma_q16_state_t *st,
                                       int32_t new_sigma_q16,
                                       int32_t k_raw_q16)
{
    int32_t delta;
    int64_t ke;
    if (!st)
        return;
    delta           = new_sigma_q16 - st->sigma;
    st->d_sigma     = delta;
    st->sigma       = new_sigma_q16;
    ke = ((int64_t)(COS_SIGMA_Q16_ONE - (int64_t)st->sigma) * (int64_t)k_raw_q16)
        >> 16;
    if (ke < 0)
        ke = 0;
    if (ke > COS_SIGMA_Q16_ONE - 1)
        ke = COS_SIGMA_Q16_ONE - 1;
    st->k_eff = (int32_t)ke;
}

/* Matches ``sigma_gate`` / ``Verdict`` in ``python/cos/sigma_gate_core.py``. */
typedef enum {
    COS_SIGMA_VERDICT_ACCEPT = 0,
    COS_SIGMA_VERDICT_RETHINK = 1,
    COS_SIGMA_VERDICT_ABSTAIN = 2,
} cos_sigma_verdict_t;

static inline cos_sigma_verdict_t cos_sigma_mirror_verdict(
    const cos_sigma_q16_state_t *st)
{
    if (!st)
        return COS_SIGMA_VERDICT_ABSTAIN;
    if (st->k_eff < COS_SIGMA_K_CRIT)
        return COS_SIGMA_VERDICT_ABSTAIN;
    if (st->d_sigma > 0)
        return COS_SIGMA_VERDICT_RETHINK;
    return COS_SIGMA_VERDICT_ACCEPT;
}

#endif /* COS_SIGMA_MIRROR_H */
