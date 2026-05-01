/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Continual primitive. */

#include "continual.h"

#include <string.h>

static float safe_sq(float x) {
    if (x != x)                                   return 0.0f;  /* NaN */
    if (x ==  (1.0f / 0.0f))                      return 0.0f;
    if (x == -(1.0f / 0.0f))                      return 0.0f;
    return x * x;
}

int cos_sigma_continual_init(cos_sigma_continual_t *c,
                             float *importance, int n,
                             float freeze_threshold,
                             float decay)
{
    if (!c || !importance)                        return -1;
    if (n <= 0)                                   return -2;
    if (!(freeze_threshold > 0.0f))               return -3;
    if (!(decay >= 0.0f && decay <= 1.0f))        return -4;
    memset(c, 0, sizeof *c);
    memset(importance, 0, sizeof(float) * (size_t)n);
    c->importance       = importance;
    c->n                = n;
    c->freeze_threshold = freeze_threshold;
    c->decay            = decay;
    return 0;
}

int cos_sigma_continual_observe_gradient(cos_sigma_continual_t *c,
                                         const float *grad)
{
    if (!c || !grad)                              return 0;
    int newly_frozen = 0;
    int frozen_now   = 0;
    float thr = c->freeze_threshold;
    for (int i = 0; i < c->n; ++i) {
        bool was_frozen = c->importance[i] > thr;
        c->importance[i] = c->decay * c->importance[i]
                         + (1.0f - c->decay) * safe_sq(grad[i]);
        bool is_frozen = c->importance[i] > thr;
        if (is_frozen) ++frozen_now;
        if (!was_frozen && is_frozen) ++newly_frozen;
    }
    c->frozen_count = frozen_now;
    ++c->n_steps;
    return newly_frozen;
}

int cos_sigma_continual_apply_mask(cos_sigma_continual_t *c,
                                   float *grad)
{
    if (!c || !grad) return 0;
    int zeros = 0;
    float thr = c->freeze_threshold;
    for (int i = 0; i < c->n; ++i) {
        if (c->importance[i] > thr) {
            grad[i] = 0.0f;
            ++zeros;
        }
    }
    c->n_masked_total += (uint64_t)zeros;
    return zeros;
}

int cos_sigma_continual_step(cos_sigma_continual_t *c, float *grad) {
    cos_sigma_continual_observe_gradient(c, grad);
    return cos_sigma_continual_apply_mask(c, grad);
}

void cos_sigma_continual_decay_tick(cos_sigma_continual_t *c) {
    if (!c) return;
    int frozen_now = 0;
    for (int i = 0; i < c->n; ++i) {
        c->importance[i] *= c->decay;
        if (c->importance[i] > c->freeze_threshold) ++frozen_now;
    }
    c->frozen_count = frozen_now;
}

/* --- self-test --------------------------------------------------------- */

static int check_init_guards(void) {
    cos_sigma_continual_t c;
    float imp[2];
    if (cos_sigma_continual_init(NULL, imp, 2, 0.1f, 0.99f) >= 0) return 10;
    if (cos_sigma_continual_init(&c, NULL, 2, 0.1f, 0.99f) >= 0) return 11;
    if (cos_sigma_continual_init(&c, imp, 0, 0.1f, 0.99f) >= 0) return 12;
    if (cos_sigma_continual_init(&c, imp, 2, 0.0f, 0.99f) >= 0) return 13;
    if (cos_sigma_continual_init(&c, imp, 2, 0.1f, 2.0f) >= 0) return 14;
    return 0;
}

#define COS_CONT_TEST_N 4
static int check_freeze_and_mask(void) {
    /* n=4 params, threshold 0.02 (above the "small-grad" impulse of
     * 0.0001 but below "medium-grad" importance of 0.025).  Grad
     * [1.0, 0.0, 0.5, 0.0] → importance (0.1, 0, 0.025, 0) → params
     * 0 and 2 frozen on step 1. */
    enum { N = COS_CONT_TEST_N };
    float imp[N];
    cos_sigma_continual_t c;
    int rc = cos_sigma_continual_init(&c, imp, N, 0.02f, 0.9f);
    if (rc != 0) return 20;

    float grad[N] = {1.0f, 0.0f, 0.5f, 0.0f};
    int zeros_first = cos_sigma_continual_step(&c, grad);
    if (zeros_first != 2) return 21;
    if (grad[0] != 0.0f || grad[2] != 0.0f) return 22;

    /* Step 2 with tiny grads across all params: frozen status is
     * sticky (importance only grows slightly); zeros should still
     * be exactly 2, and unfrozen slots (1 and 3) must retain their
     * original (non-zero) values. */
    float g2[N] = {0.01f, 0.01f, 0.01f, 0.01f};
    int zeros_second = cos_sigma_continual_step(&c, g2);
    if (zeros_second != 2) return 23;
    if (g2[0] != 0.0f || g2[2] != 0.0f) return 24;
    if (g2[1] != 0.01f || g2[3] != 0.01f) return 25;
    return 0;
}

static int check_decay_unfreezes(void) {
    /* Push a param above threshold, then repeatedly decay; it
     * should eventually fall back below threshold. */
    enum { N = 1 };
    float imp[N];
    cos_sigma_continual_t c;
    cos_sigma_continual_init(&c, imp, N, 0.05f, 0.5f);
    float big[N] = {2.0f};
    cos_sigma_continual_step(&c, big);
    /* importance = 0.5 · 0 + 0.5 · 4 = 2.0 → frozen. */
    if (c.frozen_count != 1) return 30;
    for (int i = 0; i < 10; ++i) cos_sigma_continual_decay_tick(&c);
    if (c.frozen_count != 0) return 31;  /* 2.0 · 0.5^10 ≈ 0.002 */
    return 0;
}

static int check_nan_gradient_ignored(void) {
    /* NaN / inf in grad must be treated as zero — cannot poison
     * importance (which would then freeze spuriously). */
    enum { N = 2 };
    float imp[N];
    cos_sigma_continual_t c;
    cos_sigma_continual_init(&c, imp, N, 0.0001f, 1.0f);
    float nan_v = 0.0f / 0.0f;
    float inf_v = 1.0f / 0.0f;
    float grad[N] = {nan_v, inf_v};
    int zeros = cos_sigma_continual_step(&c, grad);
    if (zeros != 0) return 40;
    if (c.importance[0] != 0.0f || c.importance[1] != 0.0f) return 41;
    if (c.frozen_count != 0) return 42;
    return 0;
}

static int check_bulk_freeze_rate(void) {
    /* 1000 LCG gradient draws over 64 params.  Roughly 50% of
     * params should eventually cross a medium threshold. */
    enum { N = 64 };
    float imp[N];
    cos_sigma_continual_t c;
    cos_sigma_continual_init(&c, imp, N, 0.10f, 0.95f);

    uint32_t state = 0xFEEDC0DEu;
    float grad[N];
    for (int it = 0; it < 500; ++it) {
        for (int i = 0; i < N; ++i) {
            state = state * 1664525u + 1013904223u;
            /* Sparse high-magnitude gradient: half params see
             * large swings, half stay near zero. */
            int big = (i & 1);
            grad[i] = big
                ? (((float)((state >> 8) & 0xFFFF) / 65535.0f) * 2.0f)
                : 0.01f;
        }
        cos_sigma_continual_step(&c, grad);
    }
    /* Expect the even-indexed params (big-grad) to mostly be frozen;
     * odd-indexed (small-grad) not.  Allow generous margin. */
    if (c.frozen_count < 20 || c.frozen_count > 40) return 50;
    return 0;
}

int cos_sigma_continual_self_test(void) {
    int rc;
    if ((rc = check_init_guards()))            return rc;
    if ((rc = check_freeze_and_mask()))        return rc;
    if ((rc = check_decay_unfreezes()))        return rc;
    if ((rc = check_nan_gradient_ignored()))   return rc;
    if ((rc = check_bulk_freeze_rate()))       return rc;
    return 0;
}
