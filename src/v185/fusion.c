/*
 * v185 σ-Multimodal-Fusion — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "fusion.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- tiny deterministic PRNG ----------------------------------- */

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) / (double)(1ULL << 53));
}

static void l2_norm(float *v, int n) {
    double s = 0;
    for (int i = 0; i < n; ++i) s += (double)v[i] * v[i];
    float d = (float)sqrt(s) + 1e-12f;
    for (int i = 0; i < n; ++i) v[i] /= d;
}

static float cos_dist(const float *a, const float *b, int n) {
    double num = 0, da = 0, db = 0;
    for (int i = 0; i < n; ++i) {
        num += (double)a[i] * b[i];
        da  += (double)a[i] * a[i];
        db  += (double)b[i] * b[i];
    }
    double sim = num / (sqrt(da) * sqrt(db) + 1e-12);
    return (float)(1.0 - sim);
}

/* ---- init + registry ------------------------------------------- */

void cos_v185_init(cos_v185_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x185FEEDULL;
    s->tau_conflict = 0.40f;
    s->tau_drop     = 0.85f;
}

void cos_v185_register(cos_v185_state_t *s,
                       const char *name,
                       const char *encoder,
                       int   native_dim,
                       const char *sigma_channel) {
    if (s->n_modalities >= COS_V185_MAX_MOD) return;
    cos_v185_modality_t *m = &s->modalities[s->n_modalities++];
    snprintf(m->name,          COS_V185_STR_MAX, "%s", name);
    snprintf(m->encoder,       COS_V185_STR_MAX, "%s", encoder);
    snprintf(m->sigma_channel, COS_V185_STR_MAX, "%s", sigma_channel);
    m->native_dim = native_dim;
    m->active     = true;
    m->last_sigma = 0.0f;
}

/* ---- fixture construction -------------------------------------- */

/* Shared-latent topic vector drives a consistent sample's modalities;
 * a conflicting sample overrides one modality with a different topic.
 * An outlier sample also inflates one modality's σ > τ_drop, forcing
 * the dynamic-drop path. */
static void topic_vec(float *out, uint64_t *rng) {
    for (int i = 0; i < COS_V185_D; ++i) out[i] = frand(rng) - 0.5f;
    l2_norm(out, COS_V185_D);
}

void cos_v185_build_samples(cos_v185_state_t *s) {
    uint64_t rng = s->seed;
    s->n_samples = COS_V185_N_SAMPLES;

    for (int i = 0; i < s->n_samples; ++i) {
        cos_v185_sample_t *sp = &s->samples[i];
        memset(sp, 0, sizeof(*sp));
        sp->id = i;

        /* Deterministic classification of samples:
         *   i % 4 == 0 → consistent (low σ)
         *   i % 4 == 1 → consistent (higher σ but aligned)
         *   i % 4 == 2 → conflicting (one modality flipped)
         *   i % 4 == 3 → outlier (one modality σ > τ_drop → dropped);
         *               remaining modalities are still consistent, so
         *               the overall sample is labelled consistent —
         *               this is the graceful-degradation contract.
         */
        int kind = i % 4;
        sp->ground_truth_consistent = (kind != 2);

        float topic[COS_V185_D];
        topic_vec(topic, &rng);

        float conflict[COS_V185_D];
        topic_vec(conflict, &rng);

        for (int m = 0; m < s->n_modalities; ++m) {
            sp->present[m] = true;

            /* Jitter the shared topic for each modality. */
            float jitter[COS_V185_D];
            for (int k = 0; k < COS_V185_D; ++k)
                jitter[k] = topic[k] + 0.05f * (frand(&rng) - 0.5f);

            float base_sigma = 0.10f + 0.04f * m;

            if (kind == 2 && m == 1) {
                /* flip modality 1 to the conflicting topic */
                for (int k = 0; k < COS_V185_D; ++k)
                    jitter[k] = conflict[k];
                base_sigma = 0.55f;
            }
            if (kind == 3 && m == 2) {
                /* outlier: modality 2 becomes noisy; σ > τ_drop */
                for (int k = 0; k < COS_V185_D; ++k)
                    jitter[k] = conflict[k] * 0.3f
                                + (frand(&rng) - 0.5f);
                base_sigma = 0.90f;
            }

            l2_norm(jitter, COS_V185_D);
            for (int k = 0; k < COS_V185_D; ++k)
                sp->proj[m][k] = jitter[k];
            sp->sigma_per[m] = base_sigma;
        }
    }
}

/* ---- fusion ---------------------------------------------------- */

static void fuse_sample(cos_v185_state_t *s, cos_v185_sample_t *sp) {
    /* Dynamic drop: σ_i > τ_drop ⇒ present = false */
    int n_active = 0;
    for (int m = 0; m < s->n_modalities; ++m) {
        if (!s->modalities[m].active) { sp->present[m] = false; continue; }
        if (sp->sigma_per[m] > s->tau_drop) {
            sp->present[m] = false;
            continue;
        }
        sp->present[m] = true;
        n_active++;
    }
    sp->n_active = n_active;

    /* Weights: w_i = 1 / (1 + σ_i) if present, else 0. */
    double sum_w = 0.0;
    for (int m = 0; m < s->n_modalities; ++m) {
        if (sp->present[m]) {
            sp->weight[m] = 1.0f / (1.0f + sp->sigma_per[m]);
            sum_w += sp->weight[m];
        } else sp->weight[m] = 0.0f;
    }
    if (sum_w < 1e-9) sum_w = 1.0;  /* no modality → identity */

    /* Normalize and fuse. */
    for (int k = 0; k < COS_V185_D; ++k) sp->fused[k] = 0.0f;
    for (int m = 0; m < s->n_modalities; ++m) {
        if (!sp->present[m]) continue;
        float w = sp->weight[m] / (float)sum_w;
        sp->weight[m] = w;
        for (int k = 0; k < COS_V185_D; ++k)
            sp->fused[k] += w * sp->proj[m][k];
    }

    /* σ_cross: mean pairwise cosine distance between present modalities. */
    double sum_d = 0; int pairs = 0;
    for (int a = 0; a < s->n_modalities; ++a) {
        if (!sp->present[a]) continue;
        for (int b = a + 1; b < s->n_modalities; ++b) {
            if (!sp->present[b]) continue;
            sum_d += cos_dist(sp->proj[a], sp->proj[b], COS_V185_D);
            pairs++;
        }
    }
    sp->sigma_cross       = (pairs == 0) ? 0.0f :
                            (float)(sum_d / pairs);
    if (sp->sigma_cross > 1.0f) sp->sigma_cross = 1.0f;
    sp->flagged_conflict  = (sp->sigma_cross >= s->tau_conflict);

    /* σ_fused: noisy-OR over present σ_i. */
    float prod = 1.0f;
    for (int m = 0; m < s->n_modalities; ++m) {
        if (sp->present[m]) prod *= (1.0f - sp->sigma_per[m]);
    }
    sp->sigma_fused = 1.0f - prod;
    if (sp->sigma_fused < 0.0f) sp->sigma_fused = 0.0f;
    if (sp->sigma_fused > 1.0f) sp->sigma_fused = 1.0f;
}

void cos_v185_run_fusion(cos_v185_state_t *s) {
    s->n_consistent_correct = 0;
    s->n_conflict_correct   = 0;
    s->n_dropped_modalities = 0;

    double sum_c = 0, sum_x = 0;
    int    n_c = 0,   n_x = 0;

    for (int i = 0; i < s->n_samples; ++i) {
        cos_v185_sample_t *sp = &s->samples[i];
        fuse_sample(s, sp);

        if (sp->ground_truth_consistent) {
            if (!sp->flagged_conflict) s->n_consistent_correct++;
            sum_c += sp->sigma_cross; n_c++;
        } else {
            if (sp->flagged_conflict) s->n_conflict_correct++;
            sum_x += sp->sigma_cross; n_x++;
        }
        for (int m = 0; m < s->n_modalities; ++m)
            if (!sp->present[m]) s->n_dropped_modalities++;
    }

    s->mean_sigma_cross_consistent =
        n_c ? (float)(sum_c / n_c) : 0.0f;
    s->mean_sigma_cross_conflict =
        n_x ? (float)(sum_x / n_x) : 0.0f;
}

/* ---- JSON ------------------------------------------------------ */

size_t cos_v185_to_json(const cos_v185_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v185\","
        "\"n_modalities\":%d,\"n_samples\":%d,"
        "\"tau_conflict\":%.3f,\"tau_drop\":%.3f,"
        "\"n_consistent_correct\":%d,\"n_conflict_correct\":%d,"
        "\"n_dropped_modalities\":%d,"
        "\"mean_sigma_cross_consistent\":%.4f,"
        "\"mean_sigma_cross_conflict\":%.4f,"
        "\"modalities\":[",
        s->n_modalities, s->n_samples,
        s->tau_conflict, s->tau_drop,
        s->n_consistent_correct, s->n_conflict_correct,
        s->n_dropped_modalities,
        s->mean_sigma_cross_consistent,
        s->mean_sigma_cross_conflict);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int m = 0; m < s->n_modalities; ++m) {
        const cos_v185_modality_t *mm = &s->modalities[m];
        int k = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"encoder\":\"%s\","
            "\"native_dim\":%d,\"sigma_channel\":\"%s\","
            "\"active\":%s}",
            m == 0 ? "" : ",", mm->name, mm->encoder,
            mm->native_dim, mm->sigma_channel,
            mm->active ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m2 = snprintf(buf + off, cap - off, "],\"samples\":[");
    if (m2 < 0 || off + (size_t)m2 >= cap) return 0;
    off += (size_t)m2;
    for (int i = 0; i < s->n_samples; ++i) {
        const cos_v185_sample_t *sp = &s->samples[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"truth\":%s,\"flagged\":%s,"
            "\"sigma_cross\":%.4f,\"sigma_fused\":%.4f,"
            "\"n_active\":%d}",
            i == 0 ? "" : ",", sp->id,
            sp->ground_truth_consistent ? "true" : "false",
            sp->flagged_conflict        ? "true" : "false",
            sp->sigma_cross, sp->sigma_fused, sp->n_active);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m3 = snprintf(buf + off, cap - off, "]}");
    if (m3 < 0 || off + (size_t)m3 >= cap) return 0;
    return off + (size_t)m3;
}

/* ---- self-test ------------------------------------------------- */

int cos_v185_self_test(void) {
    cos_v185_state_t s;
    cos_v185_init(&s, 0x185FEEDULL);
    cos_v185_register(&s, "vision", "siglip",       768,  "sigma_vis");
    cos_v185_register(&s, "audio",  "whisper",      512,  "sigma_aud");
    cos_v185_register(&s, "text",   "bitnet",      2560,  "sigma_txt");
    cos_v185_register(&s, "action", "policy_head",  128,  "sigma_act");
    if (s.n_modalities != 4) return 1;

    cos_v185_build_samples(&s);
    cos_v185_run_fusion(&s);

    /* Consistency classification: most consistent and most conflict
     * samples should be classified correctly. Require 4/4 consistent
     * and ≥ 3/4 conflict under the weights-free fixture. */
    int n_c = 0, n_x = 0;
    for (int i = 0; i < s.n_samples; ++i)
        if (s.samples[i].ground_truth_consistent) n_c++;
        else                                      n_x++;
    if (s.n_consistent_correct < n_c)      return 2;
    if (s.n_conflict_correct  <  (n_x - 1)) return 3;

    /* Cross separation. */
    if (s.mean_sigma_cross_conflict <
        s.mean_sigma_cross_consistent + 0.10f) return 4;

    /* Dynamic drop exercised at least once. */
    if (s.n_dropped_modalities < 1) return 5;

    /* Graceful degradation invariant: on kind=3 outlier samples,
     * the noisy modality 2 must have been dropped so σ_cross on
     * those samples stays finite and below τ_drop. */
    for (int i = 0; i < s.n_samples; ++i) {
        const cos_v185_sample_t *sp = &s.samples[i];
        if ((i % 4) == 3) {
            if (sp->present[2]) return 6;
            if (sp->n_active != s.n_modalities - 1) return 7;
        }
    }

    /* Weight normalization: Σ w_i ≈ 1 for every sample with ≥ 1
     * active modality. */
    for (int i = 0; i < s.n_samples; ++i) {
        const cos_v185_sample_t *sp = &s.samples[i];
        if (sp->n_active == 0) continue;
        double sum = 0;
        for (int m = 0; m < s.n_modalities; ++m)
            if (sp->present[m]) sum += sp->weight[m];
        if (sum < 0.999 || sum > 1.001) return 8;
    }
    return 0;
}
