/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-TTT primitive implementation + exhaustive self-test. */

#include "ttt.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define COS_SIGMA_TTT_NORM_EPS 1e-12f

int cos_sigma_ttt_init(cos_sigma_ttt_state_t *st,
                       float *fast, const float *slow, int n,
                       float lr, float tau_sigma, float tau_drift) {
    if (!st || !fast || !slow || n <= 0)   return -1;
    if (!(lr > 0.0f) || lr != lr)          return -2;
    if (!(tau_sigma >= 0.0f) ||
         tau_sigma > 1.0f || tau_sigma != tau_sigma)
                                           return -3;
    if (!(tau_drift > 0.0f) || tau_drift != tau_drift)
                                           return -4;
    memcpy(fast, slow, (size_t)n * sizeof(float));
    st->fast = fast;
    st->slow = slow;
    st->n = n;
    st->lr = lr;
    st->tau_sigma = tau_sigma;
    st->tau_drift = tau_drift;
    st->n_steps_total = 0;
    st->n_steps_updated = 0;
    st->n_resets = 0;
    return 0;
}

int cos_sigma_ttt_step(cos_sigma_ttt_state_t *st, float sigma,
                       const float *grad) {
    if (!st || !grad)  return -1;
    if (!st->fast)     return -2;
    st->n_steps_total++;
    /* NaN σ → skip (safe default). */
    if (sigma != sigma)          return 0;
    if (sigma < st->tau_sigma)   return 0;
    float lr = st->lr;
    for (int i = 0; i < st->n; ++i) {
        float g = grad[i];
        /* Skip NaN / inf grads component-wise so one bad entry doesn't
         * corrupt the whole fast[] buffer. */
        if (g != g) continue;
        if (g ==  (1.0f / 0.0f))   continue;
        if (g == -(1.0f / 0.0f))   continue;
        st->fast[i] -= lr * g;
    }
    st->n_steps_updated++;
    return 1;
}

float cos_sigma_ttt_drift(const cos_sigma_ttt_state_t *st) {
    if (!st || !st->fast || !st->slow || st->n <= 0) return 0.0f;
    double num2 = 0.0;
    double den2 = 0.0;
    for (int i = 0; i < st->n; ++i) {
        double d = (double)st->fast[i] - (double)st->slow[i];
        num2 += d * d;
        double s = (double)st->slow[i];
        den2 += s * s;
    }
    double num = sqrt(num2);
    double den = sqrt(den2);
    if (den < (double)COS_SIGMA_TTT_NORM_EPS) {
        /* slow[] has near-zero norm.  If fast matches, drift = 0;
         * otherwise report a large but finite drift so callers reset. */
        return (num < (double)COS_SIGMA_TTT_NORM_EPS) ? 0.0f : 1.0f;
    }
    return (float)(num / den);
}

int cos_sigma_ttt_reset_if_drift(cos_sigma_ttt_state_t *st) {
    if (!st || !st->fast || !st->slow) return -1;
    float d = cos_sigma_ttt_drift(st);
    if (!(d > st->tau_drift)) return 0;
    memcpy(st->fast, st->slow, (size_t)st->n * sizeof(float));
    st->n_resets++;
    return 1;
}

/* --- self-test --------------------------------------------------------- */

static int check_init_errors(void) {
    cos_sigma_ttt_state_t st;
    float slow[4] = {1, 2, 3, 4};
    float fast[4];
    if (cos_sigma_ttt_init(NULL, fast, slow, 4, 0.1f, 0.5f, 0.2f) >= 0)
        return 10;
    if (cos_sigma_ttt_init(&st, NULL, slow, 4, 0.1f, 0.5f, 0.2f) >= 0)
        return 11;
    if (cos_sigma_ttt_init(&st, fast, NULL, 4, 0.1f, 0.5f, 0.2f) >= 0)
        return 12;
    if (cos_sigma_ttt_init(&st, fast, slow, 0,  0.1f, 0.5f, 0.2f) >= 0)
        return 13;
    if (cos_sigma_ttt_init(&st, fast, slow, 4, -0.1f, 0.5f, 0.2f) >= 0)
        return 14;
    if (cos_sigma_ttt_init(&st, fast, slow, 4, 0.1f, 1.5f, 0.2f) >= 0)
        return 15;
    if (cos_sigma_ttt_init(&st, fast, slow, 4, 0.1f, 0.5f, 0.0f) >= 0)
        return 16;
    return 0;
}

static int check_gate_semantics(void) {
    cos_sigma_ttt_state_t st;
    float slow[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float fast[4];
    int rc = cos_sigma_ttt_init(&st, fast, slow, 4,
                                /*lr=*/0.1f, /*tau_sigma=*/0.5f,
                                /*tau_drift=*/0.25f);
    if (rc != 0) return 20;
    float grad[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    /* σ below gate → no update. */
    rc = cos_sigma_ttt_step(&st, 0.10f, grad);
    if (rc != 0)                       return 21;
    if (st.n_steps_updated != 0)       return 22;
    if (fast[0] != 1.0f)               return 23;

    /* σ == τ → update (gate is >=). */
    rc = cos_sigma_ttt_step(&st, 0.50f, grad);
    if (rc != 1)                       return 24;
    if (st.n_steps_updated != 1)       return 25;
    if (fast[0] != 0.9f)               return 26;   /* 1.0 - 0.1·1.0 */

    /* σ above gate → update. */
    rc = cos_sigma_ttt_step(&st, 0.90f, grad);
    if (rc != 1)                       return 27;
    if (fast[0] < 0.79f || fast[0] > 0.81f) return 28;

    /* NaN σ → skip. */
    float nan_v = 0.0f / 0.0f;
    rc = cos_sigma_ttt_step(&st, nan_v, grad);
    if (rc != 0)                       return 29;

    /* NaN grad entry is skipped component-wise. */
    float bad_grad[4] = {0.0f / 0.0f, 1.0f, 1.0f, 1.0f};
    float fast0_before = fast[0];
    rc = cos_sigma_ttt_step(&st, 0.90f, bad_grad);
    if (rc != 1)                       return 30;
    if (fast[0] != fast0_before)       return 31;    /* NaN entry skipped */
    return 0;
}

static int check_drift_and_reset(void) {
    cos_sigma_ttt_state_t st;
    float slow[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float fast[4];
    if (cos_sigma_ttt_init(&st, fast, slow, 4,
                           /*lr=*/0.5f, /*tau_sigma=*/0.0f,
                           /*tau_drift=*/0.20f) != 0) return 40;

    /* Drift starts at 0. */
    if (cos_sigma_ttt_drift(&st) != 0.0f) return 41;
    /* reset_if_drift on zero drift → no-op. */
    if (cos_sigma_ttt_reset_if_drift(&st) != 0) return 42;
    if (st.n_resets != 0)                     return 43;

    /* Two aggressive steps → drift > 0.20. */
    float grad[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    cos_sigma_ttt_step(&st, 1.0f, grad);
    cos_sigma_ttt_step(&st, 1.0f, grad);
    float d = cos_sigma_ttt_drift(&st);
    if (!(d > 0.20f))                         return 44;

    /* Now reset_if_drift should fire. */
    if (cos_sigma_ttt_reset_if_drift(&st) != 1) return 45;
    if (st.n_resets != 1)                     return 46;
    if (cos_sigma_ttt_drift(&st) != 0.0f)     return 47;

    /* Idempotence: second call after reset is a no-op. */
    if (cos_sigma_ttt_reset_if_drift(&st) != 0) return 48;
    if (st.n_resets != 1)                     return 49;
    return 0;
}

static int check_lcg_stability(void) {
    /* 10^4 LCG updates — fast[] stays finite OR hits at least one reset
     * event; drift never exceeds 2·tau_drift. */
    enum { N = 32 };
    cos_sigma_ttt_state_t st;
    float slow[N], fast[N];
    for (int i = 0; i < N; ++i) slow[i] = 1.0f + 0.01f * (float)i;
    if (cos_sigma_ttt_init(&st, fast, slow, N,
                           /*lr=*/0.01f, /*tau_sigma=*/0.5f,
                           /*tau_drift=*/0.30f) != 0) return 60;

    uint32_t state = 0xC0C0B00Bu;
    float grad[N];
    for (int step = 0; step < 10000; ++step) {
        state = state * 1664525u + 1013904223u;
        float sigma = (float)((state >> 8) & 0xFFFF) / 65535.0f;
        for (int i = 0; i < N; ++i) {
            state = state * 1664525u + 1013904223u;
            /* grad ∈ [-1, 1] */
            grad[i] = ((float)((state >> 8) & 0xFFFF) / 65535.0f) * 2.0f
                    - 1.0f;
        }
        cos_sigma_ttt_step(&st, sigma, grad);
        cos_sigma_ttt_reset_if_drift(&st);
        float d = cos_sigma_ttt_drift(&st);
        if (!(d <= 2.0f * st.tau_drift)) return 61;
        for (int i = 0; i < N; ++i) {
            if (fast[i] != fast[i])           return 62;
            if (fast[i] ==  (1.0f / 0.0f))    return 63;
            if (fast[i] == -(1.0f / 0.0f))    return 64;
        }
    }
    if (st.n_steps_total != 10000) return 65;
    return 0;
}

int cos_sigma_ttt_self_test(void) {
    int rc;
    if ((rc = check_init_errors()))      return rc;
    if ((rc = check_gate_semantics()))   return rc;
    if ((rc = check_drift_and_reset()))  return rc;
    if ((rc = check_lcg_stability()))    return rc;
    return 0;
}
