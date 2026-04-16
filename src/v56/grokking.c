/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "grokking.h"

#include <math.h>
#include <string.h>

#if defined(__aarch64__) || defined(__ARM_NEON)
#include <arm_neon.h>
#define V56_GROK_HAS_NEON 1
#else
#define V56_GROK_HAS_NEON 0
#endif

#if V56_GROK_HAS_NEON
/* Returns (||d||², ||a||² + ||b||²) via four parallel accumulators.
 * d = a - b.  Per-vector reduction; the caller normalizes. */
static void v56_grok_pair_norms_neon(const float *a, const float *b,
                                     int dim, float *sum_d2, float *sum_ab2)
{
    float32x4_t d0 = vdupq_n_f32(0.0f), d1 = vdupq_n_f32(0.0f);
    float32x4_t d2 = vdupq_n_f32(0.0f), d3 = vdupq_n_f32(0.0f);
    float32x4_t s0 = vdupq_n_f32(0.0f), s1 = vdupq_n_f32(0.0f);
    float32x4_t s2 = vdupq_n_f32(0.0f), s3 = vdupq_n_f32(0.0f);
    int i = 0;
    for (; i + 15 < dim; i += 16) {
        __builtin_prefetch(a + i + 64, 0, 3);
        __builtin_prefetch(b + i + 64, 0, 3);
        float32x4_t av0 = vld1q_f32(a + i + 0);
        float32x4_t av1 = vld1q_f32(a + i + 4);
        float32x4_t av2 = vld1q_f32(a + i + 8);
        float32x4_t av3 = vld1q_f32(a + i + 12);
        float32x4_t bv0 = vld1q_f32(b + i + 0);
        float32x4_t bv1 = vld1q_f32(b + i + 4);
        float32x4_t bv2 = vld1q_f32(b + i + 8);
        float32x4_t bv3 = vld1q_f32(b + i + 12);
        float32x4_t dv0 = vsubq_f32(av0, bv0);
        float32x4_t dv1 = vsubq_f32(av1, bv1);
        float32x4_t dv2 = vsubq_f32(av2, bv2);
        float32x4_t dv3 = vsubq_f32(av3, bv3);
        d0 = vfmaq_f32(d0, dv0, dv0);
        d1 = vfmaq_f32(d1, dv1, dv1);
        d2 = vfmaq_f32(d2, dv2, dv2);
        d3 = vfmaq_f32(d3, dv3, dv3);
        s0 = vfmaq_f32(s0, av0, av0);
        s1 = vfmaq_f32(s1, av1, av1);
        s2 = vfmaq_f32(s2, av2, av2);
        s3 = vfmaq_f32(s3, av3, av3);
        s0 = vfmaq_f32(s0, bv0, bv0);
        s1 = vfmaq_f32(s1, bv1, bv1);
        s2 = vfmaq_f32(s2, bv2, bv2);
        s3 = vfmaq_f32(s3, bv3, bv3);
    }
    float32x4_t dsum = vaddq_f32(vaddq_f32(d0, d1), vaddq_f32(d2, d3));
    float32x4_t ssum = vaddq_f32(vaddq_f32(s0, s1), vaddq_f32(s2, s3));
    float d_acc = vaddvq_f32(dsum);
    float s_acc = vaddvq_f32(ssum);
    for (; i < dim; i++) {
        float da = a[i] - b[i];
        d_acc += da * da;
        s_acc += a[i] * a[i] + b[i] * b[i];
    }
    *sum_d2  = d_acc;
    *sum_ab2 = s_acc;
}
#endif

#if !V56_GROK_HAS_NEON
static void v56_grok_pair_norms_scalar(const float *a, const float *b,
                                       int dim, float *sum_d2, float *sum_ab2)
{
    float d_acc = 0.0f, s_acc = 0.0f;
    for (int i = 0; i < dim; i++) {
        float da = a[i] - b[i];
        d_acc += da * da;
        s_acc += a[i] * a[i] + b[i] * b[i];
    }
    *sum_d2  = d_acc;
    *sum_ab2 = s_acc;
}
#endif

float v56_grok_defect(const float *g_new, const float *g_prev, int dim)
{
    if (!g_new || !g_prev || dim <= 0) return 0.0f;
    float d2 = 0.0f, s2 = 0.0f;
#if V56_GROK_HAS_NEON
    v56_grok_pair_norms_neon(g_new, g_prev, dim, &d2, &s2);
#else
    v56_grok_pair_norms_scalar(g_new, g_prev, dim, &d2, &s2);
#endif
    float denom = s2 + 1e-12f;
    float v = d2 / denom;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

void v56_grok_init(const v56_grok_params_t *p, v56_grok_state_t *s)
{
    (void)p;
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

void v56_grok_observe(const v56_grok_params_t *p,
                      v56_grok_state_t *s,
                      const float *g_new,
                      const float *g_prev,
                      int dim)
{
    if (!p || !s) return;
    float d = v56_grok_defect(g_new, g_prev, dim);
    s->latest_defect = d;
    if (d > s->max_defect_observed) s->max_defect_observed = d;

    /* Seed the baseline on first call. */
    if (s->step_count == 0) {
        s->baseline_defect = d;
    } else {
        float ew = p->baseline_ewma;
        if (ew <= 0.0f || ew >= 1.0f) ew = 0.98f;
        s->baseline_defect = ew * s->baseline_defect + (1.0f - ew) * d;
    }
    s->step_count++;

    if (s->step_count >= p->baseline_warmup) s->phase_transition_armed = 1;

    float mult = p->spike_multiplier > 0.0f ? p->spike_multiplier : 10.0f;
    int spike = (s->phase_transition_armed &&
                 d > s->baseline_defect * mult) ? 1 : 0;
    if (spike && !s->phase_transition_detected) s->transitions_total++;
    s->phase_transition_detected = spike;
}
