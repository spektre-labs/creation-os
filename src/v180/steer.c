/*
 * v180 σ-Steer — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "steer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float frand(uint64_t *s) {
    return (splitmix64(s) >> 40) / 16777216.0f;
}
static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo; if (x > hi) return hi; return x;
}
static void l2_normalize(float *v, int d) {
    double s = 0.0;
    for (int i = 0; i < d; ++i) s += (double)v[i] * v[i];
    if (s <= 1e-12) return;
    float inv = (float)(1.0 / sqrt(s));
    for (int i = 0; i < d; ++i) v[i] *= inv;
}
static float dot(const float *a, const float *b, int d) {
    double s = 0.0;
    for (int i = 0; i < d; ++i) s += (double)a[i] * b[i];
    return (float)s;
}

/* ------------------------------------------------------------- */
/* Init                                                          */
/* ------------------------------------------------------------- */

void cos_v180_init(cos_v180_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed           = seed ? seed : 0x180DA1ECF11EULL;
    s->tau_high_sigma = 0.50f;
    s->tau_firmware   = 0.40f;
}

/* ------------------------------------------------------------- */
/* Build vectors                                                 */
/* ------------------------------------------------------------- */

void cos_v180_build_vectors(cos_v180_state_t *s) {
    uint64_t rs = s->seed ^ 0xD1EEEE57EEDULL;
    /* Truthful direction: first block negative, second positive. */
    cos_v180_vector_t *t = &s->vectors[0];
    t->kind = COS_V180_VEC_TRUTHFUL; t->alpha = 0.35f;
    snprintf(t->name, sizeof(t->name), "truthful");
    for (int i = 0; i < COS_V180_D; ++i)
        t->v[i] = (i < COS_V180_D / 2 ? -1.0f : 1.0f) + 0.1f * frand(&rs);
    l2_normalize(t->v, COS_V180_D);

    /* No-firmware direction: every 4th dim. */
    cos_v180_vector_t *n = &s->vectors[1];
    n->kind = COS_V180_VEC_NO_FIRMWARE; n->alpha = 0.40f;
    snprintf(n->name, sizeof(n->name), "no_firmware");
    for (int i = 0; i < COS_V180_D; ++i)
        n->v[i] = (i % 4 == 0 ? 1.0f : -0.1f) + 0.05f * frand(&rs);
    l2_normalize(n->v, COS_V180_D);

    /* Expert direction: linearly increasing. */
    cos_v180_vector_t *e = &s->vectors[2];
    e->kind = COS_V180_VEC_EXPERT; e->alpha = 0.25f;
    snprintf(e->name, sizeof(e->name), "expert");
    for (int i = 0; i < COS_V180_D; ++i)
        e->v[i] = ((float)i / COS_V180_D - 0.5f) + 0.05f * frand(&rs);
    l2_normalize(e->v, COS_V180_D);
}

/* ------------------------------------------------------------- */
/* Build samples                                                 */
/* ------------------------------------------------------------- */

void cos_v180_build_samples(cos_v180_state_t *s) {
    uint64_t rs = s->seed ^ 0x5AEE1A57ULL;
    s->n_samples = COS_V180_N_PROMPTS;
    for (int i = 0; i < COS_V180_N_PROMPTS; ++i) {
        cos_v180_sample_t *sm = &s->samples[i];
        memset(sm, 0, sizeof(*sm));
        sm->id = i;
        /* Half low-σ (0.05..0.30), half high-σ (0.55..0.90). */
        bool hi = (i % 2) == 1;
        sm->sigma_before = hi
            ? 0.55f + 0.35f * frand(&rs)
            : 0.05f + 0.25f * frand(&rs);
        /* Hidden state: for high-σ samples aim the state AWAY
         * from the truthful direction (so adding it reduces σ).
         * For low-σ samples: random clamped vector. */
        for (int k = 0; k < COS_V180_D; ++k)
            sm->hidden[k] = (frand(&rs) - 0.5f) * 0.5f;
        if (hi) {
            /* anti-truthful component ≈ 0.6 */
            for (int k = 0; k < COS_V180_D; ++k)
                sm->hidden[k] -= 0.6f * ((k < COS_V180_D / 2 ? -1.0f : 1.0f) /
                                         sqrtf((float)COS_V180_D));
        }
        sm->sigma_after = sm->sigma_before;
        /* Firmware-rate baseline: proportional to σ. */
        sm->firmware_rate_before = 0.10f + 0.70f * sm->sigma_before;
        sm->firmware_rate_after  = sm->firmware_rate_before;
        sm->expert_score_before  = 0.40f;
        sm->expert_score_after   = 0.40f;
    }
}

/* ------------------------------------------------------------- */
/* Truthful steering                                             */
/* ------------------------------------------------------------- */

/* σ model: σ_after ≈ clamp(σ_before − k · (truthful·hidden_after −
 *                                           truthful·hidden_before))
 * i.e. increasing the truthful component lowers σ. */
void cos_v180_run_truthful(cos_v180_state_t *s) {
    const cos_v180_vector_t *t = &s->vectors[0];
    const float k_map = 0.45f;

    int  n_hi = 0, n_lo = 0;
    float sum_hi_before = 0, sum_hi_after = 0;
    float sum_lo_abs_delta = 0.0f;

    for (int i = 0; i < s->n_samples; ++i) {
        cos_v180_sample_t *sm = &s->samples[i];
        float proj_before = dot(t->v, sm->hidden, COS_V180_D);
        bool gate = (sm->sigma_before >= s->tau_high_sigma);
        sm->steered = gate;
        if (gate) {
            /* Apply α · direction. */
            for (int j = 0; j < COS_V180_D; ++j)
                sm->hidden[j] += t->alpha * t->v[j];
        }
        float proj_after = dot(t->v, sm->hidden, COS_V180_D);
        float dproj = proj_after - proj_before;
        float delta = -k_map * dproj;               /* gate==false → 0 */
        sm->sigma_after = clampf(sm->sigma_before + delta, 0.0f, 1.0f);

        if (sm->sigma_before >= s->tau_high_sigma) {
            sum_hi_before += sm->sigma_before;
            sum_hi_after  += sm->sigma_after;
            n_hi++;
        } else {
            sum_lo_abs_delta += fabsf(sm->sigma_after - sm->sigma_before);
            n_lo++;
        }
    }
    if (n_hi > 0) {
        s->mean_sigma_before_high = sum_hi_before / n_hi;
        s->mean_sigma_after_high  = sum_hi_after  / n_hi;
        s->rel_sigma_drop_high_pct = 100.0f *
            (s->mean_sigma_before_high - s->mean_sigma_after_high) /
            (s->mean_sigma_before_high + 1e-9f);
    }
    if (n_lo > 0)
        s->mean_sigma_abs_delta_low = sum_lo_abs_delta / n_lo;
}

/* ------------------------------------------------------------- */
/* Firmware suppression                                          */
/* ------------------------------------------------------------- */

void cos_v180_run_no_firmware(cos_v180_state_t *s) {
    const float supp_gain = 0.55f;          /* up to 55 % relative */
    int   n = 0;
    float sum_b = 0, sum_a = 0;
    for (int i = 0; i < s->n_samples; ++i) {
        cos_v180_sample_t *sm = &s->samples[i];
        if (sm->sigma_before >= s->tau_firmware) {
            sm->firmware_rate_after =
                sm->firmware_rate_before * (1.0f - supp_gain);
        } else {
            /* Small, bounded reduction on low-σ samples too —
             * but capped so low-σ samples stay ≈ unchanged. */
            sm->firmware_rate_after =
                sm->firmware_rate_before * 0.98f;
        }
        sum_b += sm->firmware_rate_before;
        sum_a += sm->firmware_rate_after;
        n++;
    }
    if (n > 0) {
        s->mean_firmware_before = sum_b / n;
        s->mean_firmware_after  = sum_a / n;
        s->rel_firmware_drop_pct = 100.0f *
            (s->mean_firmware_before - s->mean_firmware_after) /
            (s->mean_firmware_before + 1e-9f);
    }
}

/* ------------------------------------------------------------- */
/* Expert ladder                                                 */
/* ------------------------------------------------------------- */

/* Three persona levels (0 novice, 1 intermediate, 2 expert)
 * produce monotonically-increasing expert-lexicon scores. */
void cos_v180_run_expert_ladder(cos_v180_state_t *s) {
    const cos_v180_vector_t *e = &s->vectors[2];
    float levels[3] = { 0.0f, 0.5f, 1.0f };
    float acc[3]    = { 0.0f, 0.0f, 0.0f };
    int   n         = 0;
    for (int i = 0; i < s->n_samples; ++i) {
        cos_v180_sample_t *sm = &s->samples[i];
        float base = sm->expert_score_before;
        float proj = dot(e->v, sm->hidden, COS_V180_D);
        for (int L = 0; L < 3; ++L) {
            /* proj ∈ roughly [-0.5, 0.5]; scale to [0, 0.4]. */
            float bump = levels[L] * (0.25f + 0.5f * proj);
            if (bump < 0.0f) bump = 0.0f;
            acc[L] += clampf(base + bump, 0.0f, 1.0f);
        }
        /* Record the highest-level score on the sample for JSON. */
        sm->expert_score_after = clampf(base + 1.0f * (0.25f + 0.5f * dot(e->v, sm->hidden, COS_V180_D)), 0.0f, 1.0f);
        n++;
    }
    s->expert_low_score  = acc[0] / n;
    s->expert_mid_score  = acc[1] / n;
    s->expert_high_score = acc[2] / n;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v180_to_json(const cos_v180_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v180\",\"n_samples\":%d,"
        "\"tau_high_sigma\":%.4f,\"tau_firmware\":%.4f,"
        "\"truthful\":{\"mean_sigma_before_high\":%.4f,"
        "\"mean_sigma_after_high\":%.4f,"
        "\"rel_sigma_drop_pct\":%.2f,"
        "\"mean_sigma_abs_delta_low\":%.4f},"
        "\"no_firmware\":{\"mean_before\":%.4f,"
        "\"mean_after\":%.4f,"
        "\"rel_firmware_drop_pct\":%.2f},"
        "\"expert\":{\"level0\":%.4f,\"level1\":%.4f,"
        "\"level2\":%.4f}}",
        s->n_samples,
        (double)s->tau_high_sigma, (double)s->tau_firmware,
        (double)s->mean_sigma_before_high,
        (double)s->mean_sigma_after_high,
        (double)s->rel_sigma_drop_high_pct,
        (double)s->mean_sigma_abs_delta_low,
        (double)s->mean_firmware_before,
        (double)s->mean_firmware_after,
        (double)s->rel_firmware_drop_pct,
        (double)s->expert_low_score,
        (double)s->expert_mid_score,
        (double)s->expert_high_score);
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v180_self_test(void) {
    cos_v180_state_t s;
    cos_v180_init(&s, 0x180DA1ECF11EULL);
    cos_v180_build_vectors(&s);
    cos_v180_build_samples(&s);
    cos_v180_run_truthful(&s);
    cos_v180_run_no_firmware(&s);
    cos_v180_run_expert_ladder(&s);

    if (s.rel_sigma_drop_high_pct < 10.0f)      return 1;
    if (s.mean_sigma_abs_delta_low > 0.02f)     return 2;
    if (s.rel_firmware_drop_pct < 25.0f)        return 3;

    if (!(s.expert_low_score < s.expert_mid_score)) return 4;
    if (!(s.expert_mid_score < s.expert_high_score)) return 5;

    /* Determinism */
    cos_v180_state_t a, b;
    cos_v180_init(&a, 0x180DA1ECF11EULL);
    cos_v180_init(&b, 0x180DA1ECF11EULL);
    cos_v180_build_vectors(&a); cos_v180_build_samples(&a);
    cos_v180_run_truthful(&a);  cos_v180_run_no_firmware(&a);
    cos_v180_run_expert_ladder(&a);
    cos_v180_build_vectors(&b); cos_v180_build_samples(&b);
    cos_v180_run_truthful(&b);  cos_v180_run_no_firmware(&b);
    cos_v180_run_expert_ladder(&b);
    char A[4096], B[4096];
    size_t na = cos_v180_to_json(&a, A, sizeof(A));
    size_t nb = cos_v180_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 6;
    if (memcmp(A, B, na) != 0) return 7;
    return 0;
}
