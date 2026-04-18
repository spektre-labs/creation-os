/*
 * v174 σ-Flywheel — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "flywheel.h"

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
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

const char *cos_v174_class_name(cos_v174_class_t c) {
    switch (c) {
        case COS_V174_CLASS_CHOSEN: return "chosen";
        case COS_V174_CLASS_PAIR:   return "pair";
        case COS_V174_CLASS_SKIP:   return "skip";
        default:                    return "?";
    }
}

void cos_v174_init(cos_v174_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed              = seed ? seed : 0x174F1A1BEEFULL;
    s->tau_confident     = 0.25f;
    s->tau_uncertain     = 0.60f;
    s->h_min             = 1.20f;
    s->var_min           = 0.010f;
    s->regression_slack  = 0.05f;
}

cos_v174_class_t
cos_v174_classify(const cos_v174_state_t *s, float sigma) {
    if (sigma < s->tau_confident) return COS_V174_CLASS_CHOSEN;
    if (sigma > s->tau_uncertain) return COS_V174_CLASS_PAIR;
    return COS_V174_CLASS_SKIP;
}

/* ------------------------------------------------------------- */
/* One cycle                                                     */
/* ------------------------------------------------------------- */

int cos_v174_run_cycle(cos_v174_state_t *s, float baseline) {
    memset(&s->cycle, 0, sizeof(s->cycle));
    s->cycle.benchmark_baseline = baseline;

    /* 1. Generate 50 synthetic questions across 5 clusters
     *    with deterministic σ_answer and σ_big_model. */
    uint64_t rs = s->seed ^ 0x174E11EULL;
    int cluster_counts[COS_V174_N_CLUSTERS] = {0};
    int n_chosen = 0, n_pair = 0, n_skip = 0;

    for (int i = 0; i < COS_V174_N_QUESTIONS; ++i) {
        cos_v174_sample_t *q = &s->samples[i];
        memset(q, 0, sizeof(*q));
        q->id      = i;
        q->cluster = (int)(splitmix64(&rs) % COS_V174_N_CLUSTERS);
        /* σ distribution: three-mode Bernoulli-ish so every
         * class is exercised for seed 0x174F1A1BEEF.
         *   42% → [0.00..0.22]  chosen
         *   38% → [0.62..0.95]  pair
         *   20% → [0.30..0.55]  skip
         */
        float roll = frand(&rs);
        float sig;
        if (roll < 0.42f) {
            sig = 0.02f + 0.20f * frand(&rs);      /* chosen */
        } else if (roll < 0.80f) {
            sig = 0.62f + 0.33f * frand(&rs);      /* pair   */
        } else {
            sig = 0.30f + 0.25f * frand(&rs);      /* skip   */
        }
        q->sigma_answer    = clampf(sig, 0.0f, 1.0f);
        /* big-model correction σ: typically much lower if we
         * actually asked for help (only matters for PAIR). */
        q->sigma_big_model = clampf(0.05f + 0.15f * frand(&rs), 0.0f, 1.0f);
        snprintf(q->prompt, sizeof(q->prompt),
                 "[q#%02d cluster=%d] synthetic proposer prompt",
                 i, q->cluster);

        q->cls = cos_v174_classify(s, q->sigma_answer);
        if (q->cls == COS_V174_CLASS_CHOSEN) n_chosen++;
        else if (q->cls == COS_V174_CLASS_PAIR) n_pair++;
        else n_skip++;
        cluster_counts[q->cluster]++;
    }
    s->n_samples         = COS_V174_N_QUESTIONS;
    s->cycle.n_chosen    = n_chosen;
    s->cycle.n_pair      = n_pair;
    s->cycle.n_skip      = n_skip;

    /* 2. Diversity: distinct clusters + Shannon entropy. */
    int distinct = 0;
    float H = 0.0f;
    for (int c = 0; c < COS_V174_N_CLUSTERS; ++c) {
        if (cluster_counts[c] > 0) distinct++;
        float p = (float)cluster_counts[c] / (float)COS_V174_N_QUESTIONS;
        if (p > 0.0f) H -= p * logf(p);
    }
    s->cycle.entropy_answers   = H;
    s->cycle.distinct_clusters = distinct;

    /* 3. σ-distribution variance (over all samples). */
    float mean = 0.0f;
    for (int i = 0; i < COS_V174_N_QUESTIONS; ++i)
        mean += s->samples[i].sigma_answer;
    mean /= (float)COS_V174_N_QUESTIONS;
    float var = 0.0f;
    for (int i = 0; i < COS_V174_N_QUESTIONS; ++i) {
        float d = s->samples[i].sigma_answer - mean;
        var += d * d;
    }
    var /= (float)COS_V174_N_QUESTIONS;
    s->cycle.sigma_variance = var;

    /* 4. Synthetic v143 score: rises with n_chosen + clusters. */
    float score =
        0.50f * ((float)n_chosen / (float)COS_V174_N_QUESTIONS) +
        0.30f * ((float)distinct / (float)COS_V174_N_CLUSTERS) +
        0.20f * clampf(H / 1.60f, 0.0f, 1.0f);
    s->cycle.benchmark_score = score;

    /* 5. Anti-collapse guards. */
    if (H < s->h_min) {
        s->cycle.collapse_triggered = true;
        snprintf(s->cycle.collapse_reason, sizeof(s->cycle.collapse_reason),
                 "entropy H=%.3f < H_min=%.3f", (double)H, (double)s->h_min);
        return 1;
    }
    if (var < s->var_min) {
        s->cycle.collapse_triggered = true;
        snprintf(s->cycle.collapse_reason, sizeof(s->cycle.collapse_reason),
                 "σ variance %.4f < var_min=%.4f",
                 (double)var, (double)s->var_min);
        return 2;
    }
    if (baseline > 0.0f && score < baseline - s->regression_slack) {
        s->cycle.collapse_triggered = true;
        snprintf(s->cycle.collapse_reason, sizeof(s->cycle.collapse_reason),
                 "score %.3f < baseline %.3f − slack %.3f",
                 (double)score, (double)baseline,
                 (double)s->regression_slack);
        return 3;
    }
    return 0;
}

/* ------------------------------------------------------------- */
/* JSON / NDJSON                                                 */
/* ------------------------------------------------------------- */

size_t cos_v174_to_json(const cos_v174_state_t *s, char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v174\",\"tau_confident\":%.4f,"
        "\"tau_uncertain\":%.4f,\"h_min\":%.4f,\"var_min\":%.4f,"
        "\"n_samples\":%d,\"n_chosen\":%d,\"n_pair\":%d,\"n_skip\":%d,"
        "\"entropy\":%.4f,\"sigma_variance\":%.4f,"
        "\"distinct_clusters\":%d,\"benchmark_score\":%.4f,"
        "\"benchmark_baseline\":%.4f,\"collapse_triggered\":%s,"
        "\"collapse_reason\":\"%s\"}",
        (double)s->tau_confident, (double)s->tau_uncertain,
        (double)s->h_min, (double)s->var_min,
        s->n_samples, s->cycle.n_chosen, s->cycle.n_pair,
        s->cycle.n_skip,
        (double)s->cycle.entropy_answers,
        (double)s->cycle.sigma_variance,
        s->cycle.distinct_clusters,
        (double)s->cycle.benchmark_score,
        (double)s->cycle.benchmark_baseline,
        s->cycle.collapse_triggered ? "true" : "false",
        s->cycle.collapse_reason);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v174_dpo_ndjson(const cos_v174_state_t *s,
                           char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    for (int i = 0; i < s->n_samples; ++i) {
        const cos_v174_sample_t *q = &s->samples[i];
        if (q->cls == COS_V174_CLASS_SKIP) continue;
        int n;
        if (q->cls == COS_V174_CLASS_CHOSEN) {
            n = snprintf(buf + used, cap - used,
                "{\"kernel\":\"v174\",\"id\":%d,\"class\":\"chosen\","
                "\"cluster\":%d,\"prompt\":\"%s\","
                "\"sigma_answer\":%.4f}\n",
                q->id, q->cluster, q->prompt, (double)q->sigma_answer);
        } else {
            n = snprintf(buf + used, cap - used,
                "{\"kernel\":\"v174\",\"id\":%d,\"class\":\"pair\","
                "\"cluster\":%d,\"prompt\":\"%s\","
                "\"sigma_rejected\":%.4f,\"sigma_chosen\":%.4f}\n",
                q->id, q->cluster, q->prompt,
                (double)q->sigma_answer, (double)q->sigma_big_model);
        }
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v174_self_test(void) {
    cos_v174_state_t s;
    cos_v174_init(&s, 0x174F1A1BEEFULL);

    int rc = cos_v174_run_cycle(&s, 0.0f);
    if (rc != 0) return 1;                      /* fresh cycle shouldn't collapse */

    /* Every class must be exercised. */
    if (s.cycle.n_chosen <= 0) return 2;
    if (s.cycle.n_pair   <= 0) return 3;
    if (s.cycle.n_skip   <= 0) return 4;
    if (s.cycle.n_chosen + s.cycle.n_pair + s.cycle.n_skip
        != COS_V174_N_QUESTIONS) return 5;

    /* Diversity: all 5 clusters hit. */
    if (s.cycle.distinct_clusters != COS_V174_N_CLUSTERS) return 6;
    if (s.cycle.entropy_answers <= s.h_min) return 7;
    if (s.cycle.sigma_variance  <= s.var_min) return 8;
    if (s.cycle.collapse_triggered) return 9;

    /* Regression guard fires if baseline >> score. */
    cos_v174_state_t s2;
    cos_v174_init(&s2, 0x174F1A1BEEFULL);
    int rc2 = cos_v174_run_cycle(&s2, 0.99f);   /* baseline much higher */
    if (rc2 == 0) return 10;                    /* must trigger */
    if (!s2.cycle.collapse_triggered) return 11;

    /* Collapse guard fires if H_min artificially raised. */
    cos_v174_state_t s3;
    cos_v174_init(&s3, 0x174F1A1BEEFULL);
    s3.h_min = 5.0f;                            /* impossible to meet */
    int rc3 = cos_v174_run_cycle(&s3, 0.0f);
    if (rc3 == 0) return 12;
    if (!s3.cycle.collapse_triggered) return 13;
    if (!strstr(s3.cycle.collapse_reason, "entropy")) return 14;

    /* Classifier boundaries. */
    if (cos_v174_classify(&s, 0.10f) != COS_V174_CLASS_CHOSEN) return 15;
    if (cos_v174_classify(&s, 0.45f) != COS_V174_CLASS_SKIP)   return 16;
    if (cos_v174_classify(&s, 0.80f) != COS_V174_CLASS_PAIR)   return 17;

    /* Determinism */
    cos_v174_state_t a, b;
    cos_v174_init(&a, 0x174F1A1BEEFULL);
    cos_v174_init(&b, 0x174F1A1BEEFULL);
    cos_v174_run_cycle(&a, 0.0f);
    cos_v174_run_cycle(&b, 0.0f);
    char A[16384], B[16384];
    size_t na = cos_v174_to_json(&a, A, sizeof(A));
    size_t nb = cos_v174_to_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 18;
    if (memcmp(A, B, na) != 0) return 19;
    return 0;
}
