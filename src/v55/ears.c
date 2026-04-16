/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ears.h"

#include <math.h>

static inline float v55_fminf(float a, float b) { return a < b ? a : b; }

float v55_ears_threshold(const v55_ears_params_t *p,
                         float p_target, float p_draft,
                         float sigma_knowledge)
{
    if (!p) return 1.0f;
    float base = 1.0f;
    if (p_draft > 1e-30f) base = p_target / p_draft;
    /* clamp ratio to [0, max_threshold] before relaxation so draft↑
     * edge cases can't silently drive the relaxed threshold above 1.0
     * without the EARS contribution being visible. */
    if (base < 0.0f) base = 0.0f;
    if (!isfinite(base)) base = p->max_threshold;
    float relaxed = base + p->alpha * sigma_knowledge;
    return v55_fminf(relaxed, p->max_threshold);
}

int v55_ears_accept(const v55_ears_params_t *p,
                    float p_target, float p_draft,
                    float rnd, float sigma_knowledge)
{
    float t = v55_ears_threshold(p, p_target, p_draft, sigma_knowledge);
    /* Single compare, no branch on the data path; compiler lowers to
     * FCMP + CSET on aarch64. */
    return rnd < t ? 1 : 0;
}

void v55_ears_accept_batch(const v55_ears_params_t *p,
                           const float *p_target,
                           const float *p_draft,
                           const float *rnd,
                           const float *sigma_knowledge,
                           int *accept_mask,
                           int n)
{
    if (!p || !p_target || !p_draft || !rnd || !sigma_knowledge || !accept_mask) return;
    for (int i = 0; i < n; i++) {
        accept_mask[i] = v55_ears_accept(p, p_target[i], p_draft[i],
                                         rnd[i], sigma_knowledge[i]);
    }
}
