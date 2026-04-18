/*
 * v179 σ-Interpret — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "interpret.h"

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

/* 8 "true" σ-driving features are baked with named uncertainty
 * modes; the remaining 16 are noise distractors.  Labels match
 * the kinds of uncertainty models actually exhibit. */
typedef struct {
    int         id;
    const char *label;
    float       base_corr;      /* target correlation magnitude */
} cos_v179_seed_t;

static const cos_v179_seed_t SEED_FEATURES[] = {
    {  3, "uncertainty-about-dates",        0.82f },
    {  7, "uncertainty-about-named-entities",0.78f },
    { 11, "numerical-ambiguity",            0.75f },
    { 14, "low-training-coverage",          0.70f },
    { 18, "conflicting-context",            0.68f },
    { 20, "rare-token-fallback",            0.65f },
    {  1, "confident-syntax",              -0.62f },
    {  9, "high-coverage-domain",          -0.60f },
};
#define N_SEED_FEATURES ((int)(sizeof(SEED_FEATURES)/sizeof(SEED_FEATURES[0])))

void cos_v179_init(cos_v179_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed            = seed ? seed : 0x179E5AEFEA7ULL;
    s->tau_correlated  = 0.60f;
    s->tau_high_sigma  = 0.45f;
}

/* ------------------------------------------------------------- */
/* Synthetic samples                                             */
/* ------------------------------------------------------------- */

void cos_v179_generate_samples(cos_v179_state_t *s) {
    uint64_t rs = s->seed ^ 0xA11CEFEEDULL;
    s->n_samples = COS_V179_N_PROMPTS;
    for (int i = 0; i < COS_V179_N_PROMPTS; ++i) {
        cos_v179_sample_t *sm = &s->samples[i];
        memset(sm, 0, sizeof(*sm));
        sm->id = i;
        /* Target σ spread: bimodal-ish so the high/low buckets
         * both have material populations. */
        float roll = frand(&rs);
        float sigma = (roll < 0.50f)
            ? 0.05f + 0.30f * frand(&rs)              /* low */
            : 0.55f + 0.40f * frand(&rs);             /* high */
        sm->sigma_product = clampf(sigma, 0.0f, 1.0f);

        /* Seeded features: activation = |r| · σ (+/-) with tiny jitter.
         * Noise features: uniform in [0, 0.3]. */
        for (int f = 0; f < COS_V179_N_FEATURES; ++f)
            sm->features[f] = 0.10f + 0.20f * frand(&rs);
        for (int k = 0; k < N_SEED_FEATURES; ++k) {
            int   fid  = SEED_FEATURES[k].id;
            float r    = SEED_FEATURES[k].base_corr;
            /* If r > 0 activation tracks σ; if r < 0 activation
             * tracks (1 − σ).  Adds small Gaussian-ish jitter. */
            float u = (frand(&rs) - 0.5f) * 0.08f;
            float a = (r >= 0.0f)
                ?       fabsf(r) * sm->sigma_product + (0.15f + u)
                :       fabsf(r) * (1.0f - sm->sigma_product) + (0.15f + u);
            sm->features[fid] = clampf(a, 0.0f, 1.0f);
        }

        /* Circuit coordinates: deterministic mapping. */
        sm->top_token    = (i * 7 + 13) & 31;
        sm->top_head     = (int)((splitmix64(&rs) % COS_V179_N_HEADS));
        sm->top_mlp      = (int)((splitmix64(&rs) % COS_V179_N_MLP));
        /* n_effective collapses for high σ samples. */
        sm->n_eff_before = 800 + (int)(splitmix64(&rs) % 100);
        if (sm->sigma_product >= s->tau_high_sigma) {
            sm->n_eff_after = 20 + (int)(splitmix64(&rs) % 40);
        } else {
            sm->n_eff_after = sm->n_eff_before -
                              (int)(splitmix64(&rs) % 50);
        }

        snprintf(sm->prompt, sizeof(sm->prompt),
                 "[prompt#%02d token=%d head=%d mlp=%d]",
                 i, sm->top_token, sm->top_head, sm->top_mlp);
    }
}

/* ------------------------------------------------------------- */
/* Correlation                                                   */
/* ------------------------------------------------------------- */

static float pearson_r(const cos_v179_sample_t *x, int feature_id,
                       int n, float *mean_hi, float *mean_lo,
                       float tau_high) {
    double sx = 0.0, sy = 0.0;
    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    double sum_hi = 0.0, sum_lo = 0.0;
    int    n_hi = 0, n_lo = 0;
    for (int i = 0; i < n; ++i) {
        double X = x[i].sigma_product;
        double Y = x[i].features[feature_id];
        sx += X; sy += Y;
        sxx += X * X; syy += Y * Y; sxy += X * Y;
        if (X >= tau_high) { sum_hi += Y; n_hi++; }
        else               { sum_lo += Y; n_lo++; }
    }
    double mx = sx / n, my = sy / n;
    double cov = sxy / n - mx * my;
    double vx  = sxx / n - mx * mx;
    double vy  = syy / n - my * my;
    if (vx <= 0.0 || vy <= 0.0) return 0.0f;
    if (mean_hi) *mean_hi = (float)(n_hi ? sum_hi / n_hi : 0.0);
    if (mean_lo) *mean_lo = (float)(n_lo ? sum_lo / n_lo : 0.0);
    return (float)(cov / sqrt(vx * vy));
}

static const char *lookup_label(int feature_id) {
    for (int k = 0; k < N_SEED_FEATURES; ++k)
        if (SEED_FEATURES[k].id == feature_id)
            return SEED_FEATURES[k].label;
    return "unlabeled-feature";
}

void cos_v179_fit_sae(cos_v179_state_t *s) {
    int kept = 0;
    for (int f = 0; f < COS_V179_N_FEATURES; ++f) {
        float mh = 0.0f, ml = 0.0f;
        float r = pearson_r(s->samples, f, s->n_samples,
                            &mh, &ml, s->tau_high_sigma);
        cos_v179_feature_t fe;
        memset(&fe, 0, sizeof(fe));
        fe.feature_id           = f;
        fe.correlation          = r;
        fe.mean_when_high_sigma = mh;
        fe.mean_when_low_sigma  = ml;
        const char *label = lookup_label(f);
        size_t ln = strlen(label);
        if (ln >= sizeof(fe.label)) ln = sizeof(fe.label) - 1;
        memcpy(fe.label, label, ln); fe.label[ln] = '\0';
        if (fabsf(r) >= s->tau_correlated)
            s->features[kept++] = fe;
    }
    /* Sort kept features by |r| descending. */
    for (int i = 0; i < kept; ++i)
        for (int j = i + 1; j < kept; ++j)
            if (fabsf(s->features[j].correlation) >
                fabsf(s->features[i].correlation)) {
                cos_v179_feature_t t = s->features[i];
                s->features[i] = s->features[j];
                s->features[j] = t;
            }
    s->n_features_correlated = kept;
}

/* ------------------------------------------------------------- */
/* Explain                                                       */
/* ------------------------------------------------------------- */

int cos_v179_explain(const cos_v179_state_t *s, int sample_id,
                     cos_v179_explain_t *out) {
    if (!s || !out || sample_id < 0 ||
        sample_id >= s->n_samples) return 1;
    const cos_v179_sample_t *sm = &s->samples[sample_id];

    /* Pick the feature whose activation on THIS sample is closest
     * to (|r| · σ), among the top-correlated features. */
    int best = -1;
    float best_score = -1.0f;
    for (int k = 0; k < s->n_features_correlated; ++k) {
        int fid = s->features[k].feature_id;
        float r = s->features[k].correlation;
        /* Expected activation under the model. */
        float exp_act = (r >= 0.0f)
            ? fabsf(r) * sm->sigma_product + 0.15f
            : fabsf(r) * (1.0f - sm->sigma_product) + 0.15f;
        float fit = 1.0f - fabsf(sm->features[fid] - exp_act);
        if (fit > best_score) { best_score = fit; best = k; }
    }
    if (best < 0) return 2;

    memset(out, 0, sizeof(*out));
    out->sample_id     = sample_id;
    out->feature_id    = s->features[best].feature_id;
    out->sigma_product = sm->sigma_product;
    out->token_pos     = sm->top_token;
    out->head_id       = sm->top_head;
    out->mlp_id        = sm->top_mlp;
    out->n_eff_before  = sm->n_eff_before;
    out->n_eff_after   = sm->n_eff_after;

    snprintf(out->explanation, sizeof(out->explanation),
        "sigma=%.2f - SAE feature #%d [%s] activated strongly at "
        "token position %d; attention head %d + MLP %d drive the "
        "signal; n_effective collapsed %d -> %d. See v140 for the "
        "sigma-channel this decomposes into.",
        (double)sm->sigma_product,
        out->feature_id, s->features[best].label,
        out->token_pos, out->head_id, out->mlp_id,
        out->n_eff_before, out->n_eff_after);
    return 0;
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v179_to_json(const cos_v179_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v179\",\"n_samples\":%d,"
        "\"n_features_correlated\":%d,\"tau_correlated\":%.4f,"
        "\"tau_high_sigma\":%.4f,\"features\":[",
        s->n_samples, s->n_features_correlated,
        (double)s->tau_correlated, (double)s->tau_high_sigma);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < s->n_features_correlated; ++i) {
        const cos_v179_feature_t *f = &s->features[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"id\":%d,\"label\":\"%s\",\"r\":%.4f,"
            "\"mean_high_sigma\":%.4f,\"mean_low_sigma\":%.4f}",
            i == 0 ? "" : ",",
            f->feature_id, f->label, (double)f->correlation,
            (double)f->mean_when_high_sigma,
            (double)f->mean_when_low_sigma);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v179_explain_json(const cos_v179_explain_t *e,
                              char *buf, size_t cap) {
    if (!e || !buf) return 0;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v179\",\"sample_id\":%d,\"feature_id\":%d,"
        "\"sigma_product\":%.4f,\"token_pos\":%d,\"head_id\":%d,"
        "\"mlp_id\":%d,\"n_eff_before\":%d,\"n_eff_after\":%d,"
        "\"explanation\":\"%s\"}",
        e->sample_id, e->feature_id, (double)e->sigma_product,
        e->token_pos, e->head_id, e->mlp_id,
        e->n_eff_before, e->n_eff_after, e->explanation);
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v179_self_test(void) {
    cos_v179_state_t s;
    cos_v179_init(&s, 0x179E5AEFEA7ULL);
    cos_v179_generate_samples(&s);
    cos_v179_fit_sae(&s);

    /* ≥ 5 features correlated with σ above τ_correlated. */
    if (s.n_features_correlated < 5) return 1;

    /* Top feature has a non-empty label that is NOT "unlabeled". */
    if (s.features[0].label[0] == '\0') return 2;
    if (strcmp(s.features[0].label, "unlabeled-feature") == 0) return 3;

    /* Top feature |r| ≥ 0.60 and matches the fixture top-|r|
     * ("uncertainty-about-dates", r=0.82). */
    if (fabsf(s.features[0].correlation) < 0.60f) return 4;
    if (s.features[0].feature_id != 3) return 5;

    /* Explain a known high-σ sample. */
    int   hi_idx = -1;
    for (int i = 0; i < s.n_samples; ++i)
        if (s.samples[i].sigma_product >= s.tau_high_sigma) {
            hi_idx = i; break;
        }
    if (hi_idx < 0) return 6;
    cos_v179_explain_t ex;
    if (cos_v179_explain(&s, hi_idx, &ex) != 0) return 7;
    /* Circuit chain populated. */
    if (ex.head_id < 0 || ex.head_id >= COS_V179_N_HEADS) return 8;
    if (ex.mlp_id  < 0 || ex.mlp_id  >= COS_V179_N_MLP)   return 9;
    /* Explanation non-empty and mentions the feature id + σ. */
    if (ex.explanation[0] == '\0') return 10;
    char needle_feat[16];
    snprintf(needle_feat, sizeof(needle_feat), "#%d", ex.feature_id);
    if (!strstr(ex.explanation, needle_feat)) return 11;
    if (!strstr(ex.explanation, "sigma="))    return 12;
    /* n_effective collapse recorded. */
    if (ex.n_eff_after >= ex.n_eff_before)    return 13;

    /* Determinism */
    cos_v179_state_t a, b;
    cos_v179_init(&a, 0x179E5AEFEA7ULL);
    cos_v179_init(&b, 0x179E5AEFEA7ULL);
    cos_v179_generate_samples(&a); cos_v179_fit_sae(&a);
    cos_v179_generate_samples(&b); cos_v179_fit_sae(&b);
    char A[16384], B[16384];
    size_t na = cos_v179_to_json(&a, A, sizeof(A));
    size_t nb = cos_v179_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 14;
    if (memcmp(A, B, na) != 0) return 15;

    return 0;
}
