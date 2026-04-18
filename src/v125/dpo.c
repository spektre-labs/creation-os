/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v125 σ-DPO — implementation (pure C, no MLX).
 *
 * Layout:
 *   - label_dataset  :  σ → preference pairs
 *   - dpo_loss       :  numerically stable softplus(−δ)
 *   - distribution   :  mean/stddev/Shannon entropy (10 bins)
 *   - mode_collapse  :  ratio test against `before` distribution
 *
 * The self-test covers: known-DPO analytical limits (preferred →
 * zero loss, anti-preferred → large loss, β → 0 → L = log 2), label-
 * dataset correction and σ-pair rules, mode-collapse detection on
 * both healthy and pathological distributions.
 */
#include "dpo.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cos_v125_config_defaults(cos_v125_config_t *cfg) {
    if (!cfg) return;
    cfg->tau_low               = COS_V125_TAU_LOW_DEFAULT;
    cfg->tau_high              = COS_V125_TAU_HIGH_DEFAULT;
    cfg->beta                  = COS_V125_BETA_DEFAULT;
    cfg->std_collapse_ratio    = COS_V125_STD_COLLAPSE_DEF;
    cfg->entropy_collapse_ratio= COS_V125_ENT_COLLAPSE_DEF;
}

/* ====================================================================
 * Dataset labeling
 * ================================================================== */

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    size_t n = strnlen(src, cap - 1);
    memcpy(dst, src, n);
    dst[n] = 0;
}

int cos_v125_label_dataset(const cos_v125_config_t *cfg,
                           const cos_v125_interaction_t *items,
                           int n,
                           cos_v125_pair_t *out,
                           int max_pairs,
                           cos_v125_dataset_stats_t *stats) {
    if (!cfg || !items || !out || !stats || max_pairs <= 0 || n <= 0)
        return 0;
    memset(stats, 0, sizeof *stats);

    /* Mark low-σ rows "used" once paired so we don't double-consume. */
    int *low_used = (int*)calloc((size_t)n, sizeof(int));
    if (!low_used) return 0;

    int emitted = 0;
    double sum_sc = 0.0, sum_sr = 0.0;

    for (int i = 0; i < n && emitted < max_pairs; ++i) {
        stats->interactions_read++;
        const cos_v125_interaction_t *it = &items[i];

        /* Rule A: correction pair. */
        if (it->user_corrected && it->correction[0]) {
            cos_v125_pair_t *p = &out[emitted++];
            safe_copy(p->prompt,   sizeof p->prompt,   it->prompt);
            safe_copy(p->chosen,   sizeof p->chosen,   it->correction);
            safe_copy(p->rejected, sizeof p->rejected, it->response);
            p->sigma_chosen   = 0.10f;              /* assumed confident */
            p->sigma_rejected = it->sigma_product;  /* observed */
            p->source         = COS_V125_PAIR_CORRECTION;
            stats->pairs_from_correction++;
            stats->pairs_emitted++;
            sum_sc += p->sigma_chosen;
            sum_sr += p->sigma_rejected;
            continue;
        }

        /* Rule B: σ pair.  "Rejected" candidate is high-σ. */
        if (it->sigma_product > cfg->tau_high) {
            int match = -1;
            for (int j = 0; j < n; ++j) {
                if (j == i) continue;
                if (low_used[j]) continue;
                const cos_v125_interaction_t *jt = &items[j];
                if (jt->sigma_product >= cfg->tau_low) continue;
                if (strncmp(jt->context_key, it->context_key,
                            COS_V125_CONTEXT_KEY) != 0) continue;
                match = j;
                break;
            }
            if (match < 0) {
                stats->rows_skipped_unpaired++;
                continue;
            }
            low_used[match] = 1;
            const cos_v125_interaction_t *jt = &items[match];
            cos_v125_pair_t *p = &out[emitted++];
            safe_copy(p->prompt,   sizeof p->prompt,   it->prompt);
            safe_copy(p->chosen,   sizeof p->chosen,   jt->response);
            safe_copy(p->rejected, sizeof p->rejected, it->response);
            p->sigma_chosen   = jt->sigma_product;
            p->sigma_rejected = it->sigma_product;
            p->source         = COS_V125_PAIR_SIGMA;
            stats->pairs_from_sigma++;
            stats->pairs_emitted++;
            sum_sc += p->sigma_chosen;
            sum_sr += p->sigma_rejected;
            continue;
        }

        /* Low-σ rows are anchor candidates — not emitted as pairs on
         * their own.  Mid-σ rows are skipped: no training signal. */
        if (it->sigma_product >= cfg->tau_low &&
            it->sigma_product <= cfg->tau_high) {
            stats->rows_skipped_mid_sigma++;
        }
        /* σ < tau_low rows without a high-σ match on their key are
         * legitimately unused this epoch. */
    }

    if (stats->pairs_emitted > 0) {
        stats->mean_sigma_chosen   = (float)(sum_sc / stats->pairs_emitted);
        stats->mean_sigma_rejected = (float)(sum_sr / stats->pairs_emitted);
    }
    free(low_used);
    return emitted;
}

/* ====================================================================
 * DPO loss (numerically stable softplus(−δ))
 * ================================================================== */

/* softplus(x) = log(1 + exp(x)) — numerically safe for large |x|. */
static float softplus(float x) {
    if (x > 20.0f)  return x;        /* exp underflows: log1p(exp(x)) ≈ x */
    if (x < -20.0f) return 0.0f;     /* log1p(exp(x)) → 0 */
    return logf(1.0f + expf(x));
}

static float sigmoidf(float x) {
    if (x > 20.0f)  return 1.0f;
    if (x < -20.0f) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

float cos_v125_dpo_loss(float beta,
                        const cos_v125_logprobs_t *lp,
                        float *pref_prob) {
    if (!lp) return NAN;
    float diff_policy = lp->logp_chosen   - lp->logp_rejected;
    float diff_ref    = lp->logp_ref_chosen - lp->logp_ref_rejected;
    float delta       = beta * (diff_policy - diff_ref);
    if (pref_prob) *pref_prob = sigmoidf(delta);
    return softplus(-delta);  /* = -log sigmoid(delta) */
}

float cos_v125_dpo_batch_loss(float beta,
                              const cos_v125_logprobs_t *batch,
                              int n) {
    if (!batch || n <= 0) return NAN;
    double acc = 0.0;
    for (int i = 0; i < n; ++i)
        acc += cos_v125_dpo_loss(beta, &batch[i], NULL);
    return (float)(acc / n);
}

/* ====================================================================
 * σ distribution / mode-collapse detector
 * ================================================================== */

void cos_v125_sigma_distribution(const float *sigmas, int n,
                                 cos_v125_distribution_stats_t *out) {
    if (!out) return;
    memset(out, 0, sizeof *out);
    out->n = n;
    if (!sigmas || n <= 0) return;

    /* mean + stddev */
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += sigmas[i];
    out->mean = (float)(sum / n);
    double var = 0.0;
    for (int i = 0; i < n; ++i) {
        double d = sigmas[i] - out->mean;
        var += d * d;
    }
    out->stddev = (float)sqrt(var / n);

    /* Shannon entropy over 10 bins ∈ [0,1]. */
    int counts[COS_V125_SIGMA_BINS] = {0};
    for (int i = 0; i < n; ++i) {
        float s = sigmas[i];
        if (s < 0.0f) s = 0.0f;
        if (s > 1.0f) s = 1.0f;
        int bin = (int)(s * COS_V125_SIGMA_BINS);
        if (bin >= COS_V125_SIGMA_BINS) bin = COS_V125_SIGMA_BINS - 1;
        counts[bin]++;
    }
    double H = 0.0;
    for (int b = 0; b < COS_V125_SIGMA_BINS; ++b) {
        if (counts[b] == 0) continue;
        double p = (double)counts[b] / n;
        H -= p * (log(p) / log(2.0));
    }
    out->entropy_bits = (float)H;
}

cos_v125_mode_t
cos_v125_check_mode_collapse(const cos_v125_config_t *cfg,
                             const cos_v125_distribution_stats_t *before,
                             const cos_v125_distribution_stats_t *after) {
    if (!cfg || !before || !after) return COS_V125_MODE_OK;
    /* Guard against zero divisions — treat zero-stddev before as a
     * pathological (already collapsed) baseline the caller should
     * handle separately; we return OK so we don't falsely flag. */
    if (before->stddev > 1e-6f) {
        float std_ratio = after->stddev / before->stddev;
        if (std_ratio < cfg->std_collapse_ratio)
            return COS_V125_MODE_COLLAPSE;
    }
    if (before->entropy_bits > 1e-6f) {
        float ent_ratio = after->entropy_bits / before->entropy_bits;
        if (ent_ratio < cfg->entropy_collapse_ratio)
            return COS_V125_MODE_COLLAPSE;
    }
    return COS_V125_MODE_OK;
}

/* ====================================================================
 * JSON
 * ================================================================== */

int cos_v125_stats_to_json(const cos_v125_dataset_stats_t *s,
                           const cos_v125_config_t *cfg,
                           char *out, size_t cap) {
    if (!s || !cfg || !out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"interactions_read\":%d,"
        "\"pairs_emitted\":%d,"
        "\"pairs_from_correction\":%d,"
        "\"pairs_from_sigma\":%d,"
        "\"rows_skipped_mid_sigma\":%d,"
        "\"rows_skipped_unpaired\":%d,"
        "\"mean_sigma_chosen\":%.4f,"
        "\"mean_sigma_rejected\":%.4f,"
        "\"tau_low\":%.4f,"
        "\"tau_high\":%.4f,"
        "\"beta\":%.4f,"
        "\"source\":\"v125-sigma-dpo\"}",
        s->interactions_read,
        s->pairs_emitted,
        s->pairs_from_correction,
        s->pairs_from_sigma,
        s->rows_skipped_mid_sigma,
        s->rows_skipped_unpaired,
        (double)s->mean_sigma_chosen,
        (double)s->mean_sigma_rejected,
        (double)cfg->tau_low,
        (double)cfg->tau_high,
        (double)cfg->beta);
}

int cos_v125_distribution_to_json(const cos_v125_distribution_stats_t *d,
                                  char *out, size_t cap) {
    if (!d || !out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"n\":%d,\"mean\":%.4f,\"stddev\":%.4f,\"entropy_bits\":%.4f}",
        d->n, (double)d->mean, (double)d->stddev,
        (double)d->entropy_bits);
}

/* ====================================================================
 * Self-test
 * ================================================================== */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v125 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        return 1; \
    } \
} while (0)

int cos_v125_self_test(void) {
    cos_v125_config_t cfg;
    cos_v125_config_defaults(&cfg);

    /* --- DPO loss analytical limits --------------------------------- */
    fprintf(stderr, "check-v125: DPO loss analytical limits\n");
    cos_v125_logprobs_t lp;

    /* Equal log-probs → δ=0 → L = log 2 ≈ 0.6931 */
    lp.logp_chosen = lp.logp_rejected =
    lp.logp_ref_chosen = lp.logp_ref_rejected = -3.0f;
    float L0 = cos_v125_dpo_loss(cfg.beta, &lp, NULL);
    _CHECK(fabsf(L0 - 0.6931f) < 1e-3f,
           "δ=0 → L = log 2");

    /* Policy strongly prefers chosen vs ref → L → 0 */
    lp.logp_chosen       = -0.1f;
    lp.logp_rejected     = -8.0f;
    lp.logp_ref_chosen   = -4.0f;
    lp.logp_ref_rejected = -4.0f;
    float L_pref = cos_v125_dpo_loss(1.0f, &lp, NULL);
    /* δ = 1.0 · ((−0.1−(−8.0)) − (−4 − −4)) = 1.0 · 7.9 = 7.9
     * L = softplus(−7.9) ≈ 3.7e-4                         */
    _CHECK(L_pref < 1e-2f, "strong chosen → near-zero loss");

    /* Policy strongly anti-preferred → large loss */
    lp.logp_chosen       = -8.0f;
    lp.logp_rejected     = -0.1f;
    lp.logp_ref_chosen   = -4.0f;
    lp.logp_ref_rejected = -4.0f;
    float L_anti = cos_v125_dpo_loss(1.0f, &lp, NULL);
    _CHECK(L_anti > 7.0f, "strong rejected → large loss");

    /* Symmetry: swapping chosen<->rejected should equal
     * softplus(−(−δ)) = softplus(δ) — which, evaluated at δ=7.9,
     * equals 7.9 + softplus(−7.9) ≈ 7.9 + 3.7e-4 ≈ 7.9003.     */
    _CHECK(fabsf(L_anti - 7.9f) < 1e-2f, "DPO symmetry");

    /* β = 0 → δ = 0 → L = log 2 irrespective of log-probs */
    lp.logp_chosen = -2.0f; lp.logp_rejected = -9.0f;
    lp.logp_ref_chosen = 0.0f; lp.logp_ref_rejected = 0.0f;
    float L_zero_beta = cos_v125_dpo_loss(0.0f, &lp, NULL);
    _CHECK(fabsf(L_zero_beta - 0.6931f) < 1e-3f, "β=0 → L = log 2");

    /* --- Batch loss -------------------------------------------------- */
    fprintf(stderr, "check-v125: DPO batch mean\n");
    cos_v125_logprobs_t batch[3];
    for (int i = 0; i < 3; ++i) {
        batch[i].logp_chosen       = -1.0f;
        batch[i].logp_rejected     = -3.0f;
        batch[i].logp_ref_chosen   = -2.0f;
        batch[i].logp_ref_rejected = -2.0f;
    }
    float Lb = cos_v125_dpo_batch_loss(cfg.beta, batch, 3);
    /* δ = 0.1 · (2 − 0) = 0.2; L = softplus(-0.2) ≈ 0.598 */
    _CHECK(fabsf(Lb - 0.5981f) < 1e-2f, "batch mean");

    /* --- Preference probability hits [0,1] -------------------------- */
    float pp;
    lp.logp_chosen = -0.1f; lp.logp_rejected = -10.0f;
    lp.logp_ref_chosen = lp.logp_ref_rejected = -2.0f;
    (void)cos_v125_dpo_loss(1.0f, &lp, &pp);
    _CHECK(pp > 0.999f, "pref prob → 1");

    /* --- Dataset labeling ------------------------------------------- */
    fprintf(stderr, "check-v125: σ-dataset labeling (corrections + σ-pairs)\n");
    cos_v125_interaction_t its[8];
    memset(its, 0, sizeof its);
    /* 0: correction — always emits a correction pair */
    snprintf(its[0].prompt,      sizeof its[0].prompt,      "q-legs-ant");
    snprintf(its[0].context_key, sizeof its[0].context_key, "bio-ant-legs");
    snprintf(its[0].response,    sizeof its[0].response,    "4 legs");
    its[0].sigma_product = 0.72f;
    its[0].user_corrected = 1;
    snprintf(its[0].correction,  sizeof its[0].correction,  "6 legs");
    /* 1+2: high-σ with matching low-σ anchor on same key */
    snprintf(its[1].prompt,      sizeof its[1].prompt,      "q-capital-ee");
    snprintf(its[1].context_key, sizeof its[1].context_key, "cap-ee");
    snprintf(its[1].response,    sizeof its[1].response,    "stockholm");
    its[1].sigma_product = 0.85f;
    snprintf(its[2].prompt,      sizeof its[2].prompt,      "q-capital-ee-2");
    snprintf(its[2].context_key, sizeof its[2].context_key, "cap-ee");
    snprintf(its[2].response,    sizeof its[2].response,    "tallinn");
    its[2].sigma_product = 0.12f;
    /* 3: high-σ without anchor → skipped_unpaired */
    snprintf(its[3].prompt,      sizeof its[3].prompt,      "q-moon-mass");
    snprintf(its[3].context_key, sizeof its[3].context_key, "astro-moon");
    snprintf(its[3].response,    sizeof its[3].response,    "no idea");
    its[3].sigma_product = 0.91f;
    /* 4: mid-σ → skipped */
    its[4].sigma_product = 0.47f;
    snprintf(its[4].context_key, sizeof its[4].context_key, "mid-bucket");
    /* 5: low-σ (anchor, unused standalone) */
    its[5].sigma_product = 0.10f;
    snprintf(its[5].context_key, sizeof its[5].context_key, "anchor-only");
    /* 6+7: second pair using key "cap-fr" */
    snprintf(its[6].prompt,      sizeof its[6].prompt,      "q-capital-fr");
    snprintf(its[6].context_key, sizeof its[6].context_key, "cap-fr");
    snprintf(its[6].response,    sizeof its[6].response,    "bordeaux");
    its[6].sigma_product = 0.75f;
    snprintf(its[7].context_key, sizeof its[7].context_key, "cap-fr");
    snprintf(its[7].response,    sizeof its[7].response,    "paris");
    its[7].sigma_product = 0.08f;

    cos_v125_pair_t pairs[16];
    cos_v125_dataset_stats_t ds;
    int np = cos_v125_label_dataset(&cfg, its, 8, pairs, 16, &ds);
    _CHECK(np == 3,                            "3 pairs emitted");
    _CHECK(ds.pairs_from_correction == 1,      "one correction pair");
    _CHECK(ds.pairs_from_sigma == 2,           "two σ pairs");
    _CHECK(ds.rows_skipped_mid_sigma == 1,     "one mid-σ skipped");
    _CHECK(ds.rows_skipped_unpaired == 1,      "one high-σ unpaired");
    _CHECK(pairs[0].source == COS_V125_PAIR_CORRECTION, "first is correction");
    _CHECK(strcmp(pairs[0].chosen, "6 legs") == 0, "correction chose text");
    _CHECK(pairs[1].source == COS_V125_PAIR_SIGMA,      "second is σ-pair");
    _CHECK(strcmp(pairs[1].chosen, "tallinn") == 0, "σ-pair prefers low-σ");
    _CHECK(strcmp(pairs[1].rejected, "stockholm") == 0, "σ-pair rejects high-σ");
    _CHECK(pairs[2].source == COS_V125_PAIR_SIGMA, "third is σ-pair");
    _CHECK(strcmp(pairs[2].chosen, "paris") == 0, "third chose paris");

    /* --- Distribution + collapse detection -------------------------- */
    fprintf(stderr, "check-v125: σ distribution + mode-collapse\n");
    /* Healthy "before" distribution: uniform σ ∈ {0.05..0.95} */
    float before[20], after_ok[20], after_collapse[20];
    for (int i = 0; i < 20; ++i) {
        before[i]         = 0.05f + (float)i * 0.045f;
        after_ok[i]       = before[i] + 0.01f * ((i % 2) ? 1 : -1);
        after_collapse[i] = 0.50f + 0.002f * ((i % 2) ? 1 : -1); /* nearly a delta */
    }
    cos_v125_distribution_stats_t db, da_ok, da_co;
    cos_v125_sigma_distribution(before,         20, &db);
    cos_v125_sigma_distribution(after_ok,       20, &da_ok);
    cos_v125_sigma_distribution(after_collapse, 20, &da_co);
    _CHECK(db.stddev > 0.2f,                    "healthy σ has spread");
    _CHECK(da_co.stddev < 0.05f,                "collapsed σ has tiny spread");
    _CHECK(cos_v125_check_mode_collapse(&cfg, &db, &da_ok)
           == COS_V125_MODE_OK,
           "healthy → OK");
    _CHECK(cos_v125_check_mode_collapse(&cfg, &db, &da_co)
           == COS_V125_MODE_COLLAPSE,
           "pathological → COLLAPSE");

    /* --- JSON shape ------------------------------------------------- */
    fprintf(stderr, "check-v125: JSON serialization shape\n");
    char js[512];
    int w = cos_v125_stats_to_json(&ds, &cfg, js, sizeof js);
    _CHECK(w > 0 && w < (int)sizeof js,         "stats JSON wrote");
    _CHECK(strstr(js, "\"pairs_emitted\":3") != NULL, "stats shape");
    _CHECK(strstr(js, "\"beta\":0.1000")     != NULL, "stats beta");

    fprintf(stderr, "check-v125: OK (DPO loss + σ-labeling + collapse detection)\n");
    return 0;
}
