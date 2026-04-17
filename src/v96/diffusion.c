/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v96 — σ-Diffusion implementation.
 */

#include "diffusion.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline int64_t q_abs64(int64_t x)
{
    uint64_t ux = (uint64_t)x;
    uint64_t m  = (uint64_t)(x >> 63);
    return (int64_t)((ux ^ m) - m);
}

/* Multiply two Q16.16 numbers.  Uses 64-bit intermediate, rounds toward
 * zero (arithmetic right shift on the absolute value is not needed since
 * integer division toward zero is sufficient for the invariants). */
static inline cos_v96_q16_t q_mul(cos_v96_q16_t a, cos_v96_q16_t b)
{
    int64_t p = (int64_t)a * (int64_t)b;
    return (cos_v96_q16_t)(p >> 16);
}

void cos_v96_diffuser_init(cos_v96_diffuser_t *d)
{
    memset(d, 0, sizeof(*d));
    d->sentinel = COS_V96_SENTINEL;

    /* Linear schedule α_bar[t] = Q1 − t·Q1/T.
     * α_bar[0] = Q1 (clean), α_bar[T] = 0 (pure endpoint). */
    for (uint32_t t = 0u; t <= COS_V96_T; ++t) {
        int64_t num = (int64_t)(COS_V96_T - t) * (int64_t)COS_V96_Q1;
        d->alpha_bar[t] = (cos_v96_q16_t)(num / (int64_t)COS_V96_T);
    }
    d->step = COS_V96_T;
    d->last_distance_to_x0 = INT64_MAX;
}

void cos_v96_set_endpoints(cos_v96_diffuser_t *d,
                           const cos_v96_q16_t x0[COS_V96_D],
                           const cos_v96_q16_t x1[COS_V96_D])
{
    memcpy(d->x0, x0, sizeof(d->x0));
    memcpy(d->x1, x1, sizeof(d->x1));
    /* Start at full corruption (t = T):
     *     α_bar[T] = 0  →  x = x1                                         */
    cos_v96_forward_to(d, COS_V96_T);
}

void cos_v96_forward_to(cos_v96_diffuser_t *d, uint32_t t)
{
    if (t > COS_V96_T) t = COS_V96_T;
    cos_v96_q16_t a = d->alpha_bar[t];
    cos_v96_q16_t b = (cos_v96_q16_t)((int32_t)COS_V96_Q1 - (int32_t)a);
    for (uint32_t i = 0u; i < COS_V96_D; ++i) {
        int32_t term0 = q_mul(a, d->x0[i]);
        int32_t term1 = q_mul(b, d->x1[i]);
        d->x[i] = (cos_v96_q16_t)(term0 + term1);
    }
    d->step = t;
    d->last_distance_to_x0 = cos_v96_distance_to_x0(d);
}

/* Deterministic DDIM-style reverse: absolute recomputation along the
 * fixed straight-line path.  Equivalent to Euler with step Q1/T but
 * free of accumulating truncation drift because α_bar[t−1] is looked
 * up once per step from the schedule.
 *
 *     x_{t−1} = α_bar[t−1] · x0 + (Q1 − α_bar[t−1]) · x1
 *
 * The forward and reverse code paths share the same `q_mul`, so
 * forward(t) ∘ reverse_run(t) lands back on x0 to within a rounding
 * error of ≤ 2 per dimension. */
void cos_v96_reverse_step(cos_v96_diffuser_t *d)
{
    if (d->step == 0u) return;                   /* already at x0 */
    uint32_t t_next = d->step - 1u;
    cos_v96_q16_t a = d->alpha_bar[t_next];
    cos_v96_q16_t b = (cos_v96_q16_t)((int32_t)COS_V96_Q1 - (int32_t)a);
    for (uint32_t i = 0u; i < COS_V96_D; ++i) {
        int32_t term0 = q_mul(a, d->x0[i]);
        int32_t term1 = q_mul(b, d->x1[i]);
        d->x[i] = (cos_v96_q16_t)(term0 + term1);
    }
    d->step = t_next;

    int64_t cur = cos_v96_distance_to_x0(d);
    if (d->last_distance_to_x0 != INT64_MAX && cur > d->last_distance_to_x0) {
        ++d->monotone_violations;
    }
    d->last_distance_to_x0 = cur;
}

void cos_v96_reverse_run(cos_v96_diffuser_t *d, uint32_t steps)
{
    for (uint32_t i = 0u; i < steps; ++i) {
        cos_v96_reverse_step(d);
    }
}

int64_t cos_v96_distance_to_x0(const cos_v96_diffuser_t *d)
{
    int64_t s = 0;
    for (uint32_t i = 0u; i < COS_V96_D; ++i) {
        int64_t delta = (int64_t)d->x[i] - (int64_t)d->x0[i];
        s += q_abs64(delta);
    }
    return s;
}

uint32_t cos_v96_schedule_ok(const cos_v96_diffuser_t *d)
{
    uint32_t ok = 1u;
    /* Strictly decreasing. */
    for (uint32_t t = 0u; t < COS_V96_T; ++t) {
        if (d->alpha_bar[t + 1u] >= d->alpha_bar[t]) ok = 0u;
    }
    /* End points. */
    if (d->alpha_bar[0]          != COS_V96_Q1) ok = 0u;
    if (d->alpha_bar[COS_V96_T]  != 0)          ok = 0u;
    return ok;
}

uint32_t cos_v96_ok(const cos_v96_diffuser_t *d)
{
    uint32_t sent   = (d->sentinel == COS_V96_SENTINEL) ? 1u : 0u;
    uint32_t sched  = cos_v96_schedule_ok(d);
    uint32_t mono   = (d->monotone_violations == 0u) ? 1u : 0u;
    uint32_t idok   = (d->identity_violations == 0u) ? 1u : 0u;
    return sent & sched & mono & idok;
}

uint32_t cos_v96_compose_decision(uint32_t v95_composed_ok, uint32_t v96_ok)
{
    return v95_composed_ok & v96_ok;
}
