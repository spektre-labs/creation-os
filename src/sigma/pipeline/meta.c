/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Meta primitive — per-domain competence bookkeeping. */

#include "meta.h"

#include <string.h>

static float clamp01f(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

int cos_sigma_meta_init(cos_meta_store_t *s,
                        cos_meta_domain_t *storage, int cap)
{
    if (!s || !storage || cap <= 0) return -1;
    memset(s, 0, sizeof *s);
    s->slots         = storage;
    s->cap           = cap;
    s->weakest_idx   = -1;
    s->strongest_idx = -1;
    memset(storage, 0, sizeof(cos_meta_domain_t) * (size_t)cap);
    for (int i = 0; i < cap; ++i) {
        storage[i].sigma_min = 1.0f;
        storage[i].sigma_max = 0.0f;
    }
    return 0;
}

static int find_or_create(cos_meta_store_t *s, const char *domain) {
    for (int i = 0; i < s->count; ++i) {
        if (strncmp(s->slots[i].domain, domain, COS_META_DOMAIN_CAP - 1) == 0)
            return i;
    }
    if (s->count >= s->cap) return -1;
    int idx = s->count++;
    memset(&s->slots[idx], 0, sizeof(cos_meta_domain_t));
    strncpy(s->slots[idx].domain, domain, COS_META_DOMAIN_CAP - 1);
    s->slots[idx].domain[COS_META_DOMAIN_CAP - 1] = '\0';
    s->slots[idx].sigma_min = 1.0f;
    s->slots[idx].sigma_max = 0.0f;
    return idx;
}

int cos_sigma_meta_observe(cos_meta_store_t *s,
                           const char *domain, float sigma)
{
    if (!s || !domain || domain[0] == '\0') return -1;
    int idx = find_or_create(s, domain);
    if (idx < 0) return -2;
    sigma = clamp01f(sigma);
    cos_meta_domain_t *b = &s->slots[idx];
    b->sigma_sum  += sigma;
    b->n_samples  += 1;
    if (sigma < b->sigma_min) b->sigma_min = sigma;
    if (sigma > b->sigma_max) b->sigma_max = sigma;
    /* Slope estimator: bucket samples into halves by *arrival
     * count* (not by timestamp).  With M samples, indices 1..⌈M/2⌉
     * are first-half, rest are second-half.  The boundary moves
     * as more samples arrive; every observation re-checks. */
    int n = b->n_samples;
    int half = (n + 1) / 2;
    if (n == half) {
        /* We just crossed into the first half boundary — this
         * single sample completes the first half. */
        b->sigma_sum_firsthalf += sigma;
        b->n_firsthalf         += 1;
    } else if (n > 2 * half) {
        b->sigma_sum_secondhalf += sigma;
        b->n_secondhalf         += 1;
    } else {
        /* Default: split by midpoint. */
        if (n <= half) {
            b->sigma_sum_firsthalf += sigma;
            b->n_firsthalf         += 1;
        } else {
            b->sigma_sum_secondhalf += sigma;
            b->n_secondhalf         += 1;
        }
    }
    s->total_samples++;
    return 0;
}

static cos_meta_competence_t competence_from_sigma(float mean) {
    if (mean < 0.30f) return COS_META_STRONG;
    if (mean < 0.50f) return COS_META_MODERATE;
    if (mean < 0.70f) return COS_META_WEAK;
    return COS_META_LIMITED;
}

void cos_sigma_meta_assess(cos_meta_store_t *s) {
    if (!s) return;
    float best_mean  = 2.0f;
    float worst_mean = -1.0f;
    int   best_idx   = -1;
    int   worst_idx  = -1;
    double total_sum = 0.0;
    int    total_n   = 0;
    for (int i = 0; i < s->count; ++i) {
        cos_meta_domain_t *b = &s->slots[i];
        if (b->n_samples == 0) {
            b->sigma_mean = 0.0f;
            b->slope      = 0.0f;
            b->competence = COS_META_STRONG;
            b->trend      = COS_META_TREND_STABLE;
            continue;
        }
        b->sigma_mean = b->sigma_sum / (float)b->n_samples;
        total_sum += (double)b->sigma_sum;
        total_n   += b->n_samples;

        float m1 = b->n_firsthalf  > 0
            ? b->sigma_sum_firsthalf  / (float)b->n_firsthalf  : b->sigma_mean;
        float m2 = b->n_secondhalf > 0
            ? b->sigma_sum_secondhalf / (float)b->n_secondhalf : b->sigma_mean;
        b->slope = m2 - m1;
        if      (b->slope < -0.05f) b->trend = COS_META_TREND_LEARNING;
        else if (b->slope >  0.05f) b->trend = COS_META_TREND_DRIFTING;
        else                         b->trend = COS_META_TREND_STABLE;

        b->competence = competence_from_sigma(b->sigma_mean);

        if (b->sigma_mean < best_mean)  { best_mean  = b->sigma_mean; best_idx  = i; }
        if (b->sigma_mean > worst_mean) { worst_mean = b->sigma_mean; worst_idx = i; }
    }
    s->strongest_idx      = best_idx;
    s->weakest_idx        = worst_idx;
    s->overall_mean_sigma = total_n > 0
        ? (float)(total_sum / (double)total_n) : 0.0f;
}

const cos_meta_domain_t *cos_sigma_meta_recommend_focus(
    const cos_meta_store_t *s)
{
    if (!s || s->count == 0 || s->weakest_idx < 0) return NULL;
    return &s->slots[s->weakest_idx];
}

const char *cos_sigma_meta_competence_name(cos_meta_competence_t c) {
    switch (c) {
        case COS_META_STRONG:   return "STRONG";
        case COS_META_MODERATE: return "MODERATE";
        case COS_META_WEAK:     return "WEAK";
        case COS_META_LIMITED:  return "LIMITED";
        default: return "?";
    }
}

const char *cos_sigma_meta_trend_name(cos_meta_trend_t t) {
    switch (t) {
        case COS_META_TREND_STABLE:   return "STABLE";
        case COS_META_TREND_LEARNING: return "LEARNING";
        case COS_META_TREND_DRIFTING: return "DRIFTING";
        default: return "?";
    }
}

/* ---------- self-test ---------- */

static float fabsf3(float x) { return x < 0.0f ? -x : x; }

static int check_guards(void) {
    cos_meta_store_t store;
    cos_meta_domain_t slots[2];
    if (cos_sigma_meta_init(&store, slots, 2) != 0) return 10;
    if (cos_sigma_meta_observe(&store, NULL, 0.5f) == 0) return 11;
    if (cos_sigma_meta_observe(&store, "",   0.5f) == 0) return 12;
    if (cos_sigma_meta_observe(&store, "a",  0.5f) != 0) return 13;
    if (cos_sigma_meta_observe(&store, "b",  0.5f) != 0) return 14;
    if (cos_sigma_meta_observe(&store, "c",  0.5f) != -2) return 15;
    return 0;
}

static int check_competence_and_focus(void) {
    cos_meta_store_t store;
    cos_meta_domain_t slots[4];
    cos_sigma_meta_init(&store, slots, 4);

    /* factual_qa: strong (σ ≈ 0.18) */
    cos_sigma_meta_observe(&store, "factual_qa", 0.15f);
    cos_sigma_meta_observe(&store, "factual_qa", 0.20f);
    cos_sigma_meta_observe(&store, "factual_qa", 0.18f);
    /* math: weak (σ ≈ 0.65) */
    cos_sigma_meta_observe(&store, "math", 0.65f);
    cos_sigma_meta_observe(&store, "math", 0.60f);
    cos_sigma_meta_observe(&store, "math", 0.70f);
    /* code: moderate (σ ≈ 0.35) */
    cos_sigma_meta_observe(&store, "code", 0.35f);
    cos_sigma_meta_observe(&store, "code", 0.40f);
    cos_sigma_meta_observe(&store, "code", 0.30f);

    cos_sigma_meta_assess(&store);

    if (store.count != 3)                                     return 20;
    if (store.total_samples != 9)                             return 21;
    if (store.strongest_idx != 0)                             return 22;  /* factual */
    if (store.weakest_idx   != 1)                             return 23;  /* math    */

    if (store.slots[0].competence != COS_META_STRONG)         return 24;
    if (store.slots[1].competence != COS_META_WEAK)           return 25;
    if (store.slots[2].competence != COS_META_MODERATE)       return 26;

    /* Means pinned. */
    if (fabsf3(store.slots[0].sigma_mean - 0.1766667f) > 1e-4f) return 27;
    if (fabsf3(store.slots[1].sigma_mean - 0.6500000f) > 1e-4f) return 28;
    if (fabsf3(store.slots[2].sigma_mean - 0.3500000f) > 1e-4f) return 29;

    /* recommend_focus → worst bucket (math). */
    const cos_meta_domain_t *focus = cos_sigma_meta_recommend_focus(&store);
    if (!focus)                                                return 30;
    if (strcmp(focus->domain, "math") != 0)                    return 31;
    return 0;
}

static int check_trend_detection(void) {
    cos_meta_store_t store;
    cos_meta_domain_t slots[2];
    cos_sigma_meta_init(&store, slots, 2);

    /* LEARNING: σ falls 0.70 → 0.10 */
    cos_sigma_meta_observe(&store, "learner", 0.70f);
    cos_sigma_meta_observe(&store, "learner", 0.60f);
    cos_sigma_meta_observe(&store, "learner", 0.50f);
    cos_sigma_meta_observe(&store, "learner", 0.20f);
    cos_sigma_meta_observe(&store, "learner", 0.15f);
    cos_sigma_meta_observe(&store, "learner", 0.10f);
    /* DRIFTING: σ rises 0.10 → 0.70 */
    cos_sigma_meta_observe(&store, "drifter", 0.10f);
    cos_sigma_meta_observe(&store, "drifter", 0.15f);
    cos_sigma_meta_observe(&store, "drifter", 0.20f);
    cos_sigma_meta_observe(&store, "drifter", 0.50f);
    cos_sigma_meta_observe(&store, "drifter", 0.60f);
    cos_sigma_meta_observe(&store, "drifter", 0.70f);

    cos_sigma_meta_assess(&store);

    if (store.slots[0].trend != COS_META_TREND_LEARNING) return 40;
    if (store.slots[1].trend != COS_META_TREND_DRIFTING) return 41;
    /* Slopes sign-checked. */
    if (!(store.slots[0].slope < -0.1f))                 return 42;
    if (!(store.slots[1].slope >  0.1f))                 return 43;
    return 0;
}

static int check_names(void) {
    if (strcmp(cos_sigma_meta_competence_name(COS_META_STRONG), "STRONG") != 0)     return 50;
    if (strcmp(cos_sigma_meta_competence_name(COS_META_LIMITED), "LIMITED") != 0)   return 51;
    if (strcmp(cos_sigma_meta_trend_name(COS_META_TREND_LEARNING), "LEARNING") != 0) return 52;
    return 0;
}

int cos_sigma_meta_self_test(void) {
    int rc;
    if ((rc = check_guards()))                  return rc;
    if ((rc = check_competence_and_focus()))    return rc;
    if ((rc = check_trend_detection()))         return rc;
    if ((rc = check_names()))                   return rc;
    return 0;
}
