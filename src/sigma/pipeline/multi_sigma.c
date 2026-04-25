/* σ-pipeline: multi-σ ensemble implementation (SCI-5). */
#include "multi_sigma.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cos_multi_sigma_weights_t cos_multi_sigma_default_weights(void) {
    cos_multi_sigma_weights_t w = {
        .w_logprob     = 0.50f,
        .w_entropy     = 0.20f,
        .w_perplexity  = 0.10f,
        .w_consistency = 0.20f,
    };
    return w;
}

static float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/* ========================================================================
 * σ_logprob — max per-token (1 − exp(logprob_selected)).
 * ======================================================================== */
float cos_multi_sigma_from_logprob(const float *logprob_selected,
                                   int n_tokens) {
    if (!logprob_selected || n_tokens <= 0) return -1.0f;
    float s_max = 0.0f;
    for (int t = 0; t < n_tokens; ++t) {
        float p = expf(logprob_selected[t]);
        float s = 1.0f - p;
        if (s > s_max) s_max = s;
    }
    return clamp01(s_max);
}

/* ========================================================================
 * σ_entropy — mean normalised per-token Shannon entropy.
 *
 * For each token row:
 *   1. Compute softmax of the log-probs (numerically stable: subtract
 *      the row-max first).
 *   2. H = −Σ p log p.
 *   3. Normalise by log(k) so H ∈ [0, 1].
 * Finally average across tokens.
 * ======================================================================== */
float cos_multi_sigma_entropy(const float * const *top_logprobs_row,
                              const int *top_n,
                              int n_tokens) {
    if (!top_logprobs_row || !top_n || n_tokens <= 0) return -1.0f;
    double acc = 0.0;
    int counted = 0;
    for (int t = 0; t < n_tokens; ++t) {
        const float *row = top_logprobs_row[t];
        int k = top_n[t];
        if (!row || k < 2) continue; /* k=1 has zero entropy */

        /* Numerically stable softmax. */
        float m = row[0];
        for (int i = 1; i < k; ++i) if (row[i] > m) m = row[i];
        double Z = 0.0;
        for (int i = 0; i < k; ++i) Z += exp((double)(row[i] - m));
        if (Z <= 0.0) continue;
        double H = 0.0;
        for (int i = 0; i < k; ++i) {
            double p = exp((double)(row[i] - m)) / Z;
            if (p > 0.0) H -= p * log(p);
        }
        double Hmax = log((double)k); /* uniform = max */
        double Hn = (Hmax > 0.0) ? (H / Hmax) : 0.0;
        acc += Hn;
        counted += 1;
    }
    if (counted == 0) return 0.0f;
    return clamp01((float)(acc / (double)counted));
}

/* ========================================================================
 * σ_perplexity — 1 − exp(mean_logprob), clipped.
 * ======================================================================== */
float cos_multi_sigma_perplexity(const float *logprob_selected,
                                 int n_tokens) {
    if (!logprob_selected || n_tokens <= 0) return -1.0f;
    double sum = 0.0;
    for (int t = 0; t < n_tokens; ++t) sum += (double)logprob_selected[t];
    double mean = sum / (double)n_tokens;
    double p = exp(mean);
    return clamp01((float)(1.0 - p));
}

/* ========================================================================
 * σ_consistency — 1 − mean pairwise Jaccard on word-bags.
 *
 * Word = maximal run of [A-Za-z0-9'].  Case-folded.  Bag = distinct
 * words.  We cap per-text bag size to keep memory bounded.
 * ======================================================================== */
#define COS_MS_BAG_CAP 256
#define COS_MS_WORD_CAP 48

typedef struct {
    char words[COS_MS_BAG_CAP][COS_MS_WORD_CAP];
    int  n;
} word_bag_t;

static void bag_of_words(const char *text, word_bag_t *bag) {
    bag->n = 0;
    if (!text) return;
    char buf[COS_MS_WORD_CAP];
    int bi = 0;
    for (const char *p = text; ; ++p) {
        char c = *p;
        int is_word = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '\'';
        if (is_word) {
            if (bi < COS_MS_WORD_CAP - 1) {
                char lc = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
                buf[bi++] = lc;
            }
            if (*p == '\0') break;
            continue;
        }
        if (bi > 0) {
            buf[bi] = '\0';
            /* Insert if distinct. */
            int dup = 0;
            for (int i = 0; i < bag->n; ++i) {
                if (strncmp(bag->words[i], buf, COS_MS_WORD_CAP) == 0) {
                    dup = 1; break;
                }
            }
            if (!dup && bag->n < COS_MS_BAG_CAP) {
                strncpy(bag->words[bag->n], buf, COS_MS_WORD_CAP - 1);
                bag->words[bag->n][COS_MS_WORD_CAP - 1] = '\0';
                bag->n += 1;
            }
            bi = 0;
        }
        if (c == '\0') break;
    }
}

static float jaccard(const word_bag_t *a, const word_bag_t *b) {
    if (a->n == 0 && b->n == 0) return 1.0f;
    int inter = 0;
    for (int i = 0; i < a->n; ++i) {
        for (int j = 0; j < b->n; ++j) {
            if (strncmp(a->words[i], b->words[j], COS_MS_WORD_CAP) == 0) {
                inter += 1; break;
            }
        }
    }
    int uni = a->n + b->n - inter;
    if (uni == 0) return 1.0f;
    return (float)inter / (float)uni;
}

float cos_multi_sigma_consistency(const char *const *regen_texts, int k) {
    if (!regen_texts) return -1.0f;
    if (k <= 1) return 0.0f;

    word_bag_t *bags = (word_bag_t *)calloc((size_t)k, sizeof(*bags));
    if (!bags) return -1.0f;
    for (int i = 0; i < k; ++i) bag_of_words(regen_texts[i], &bags[i]);

    double acc = 0.0;
    int pairs = 0;
    for (int i = 0; i < k; ++i) {
        for (int j = i + 1; j < k; ++j) {
            acc += jaccard(&bags[i], &bags[j]);
            pairs += 1;
        }
    }
    free(bags);
    if (pairs == 0) return 0.0f;
    float mean_sim = (float)(acc / (double)pairs);
    return clamp01(1.0f - mean_sim);
}

float cos_text_jaccard(const char *a, const char *b) {
    const char *pair[2] = { a, b };
    if (!a || !b) return 0.0f;
    float cs = cos_multi_sigma_consistency(pair, 2);
    if (cs < 0.0f) return 0.0f;
    /* cos_multi_sigma_consistency = 1 − Jaccard for one pair. */
    return clamp01(1.0f - cs);
}

/* ========================================================================
 * Ensemble.
 * ======================================================================== */
int cos_multi_sigma_combine(float sigma_logprob,
                            float sigma_entropy,
                            float sigma_perplexity,
                            float sigma_consistency,
                            const cos_multi_sigma_weights_t *weights,
                            cos_multi_sigma_t *out) {
    if (!out) return -1;
    cos_multi_sigma_weights_t w = weights ? *weights
                                          : cos_multi_sigma_default_weights();

    float s_lp = clamp01(sigma_logprob    < 0.0f ? 0.0f : sigma_logprob);
    float s_en = clamp01(sigma_entropy    < 0.0f ? 0.0f : sigma_entropy);
    float s_pp = clamp01(sigma_perplexity < 0.0f ? 0.0f : sigma_perplexity);
    float s_cs = clamp01(sigma_consistency< 0.0f ? 0.0f : sigma_consistency);

    /* Renormalise weights defensively. */
    float sum = w.w_logprob + w.w_entropy + w.w_perplexity + w.w_consistency;
    if (!(sum > 0.0f)) {
        w   = cos_multi_sigma_default_weights();
        sum = w.w_logprob + w.w_entropy + w.w_perplexity + w.w_consistency;
    }
    float inv = 1.0f / sum;

    out->sigma_logprob     = s_lp;
    out->sigma_entropy     = s_en;
    out->sigma_perplexity  = s_pp;
    out->sigma_consistency = s_cs;
    out->sigma_combined    = clamp01(inv * (w.w_logprob     * s_lp +
                                            w.w_entropy     * s_en +
                                            w.w_perplexity  * s_pp +
                                            w.w_consistency * s_cs));
    return 0;
}

/* ========================================================================
 * Self-test.
 * ======================================================================== */
int cos_multi_sigma_self_test(void) {
    /* 1. σ_logprob monotonicity.
     *    Selected log-probs: one peaky token (−0.01) and one flat (−2.0). */
    float lp[2]   = { -0.01f, -2.0f };
    float s_lp    = cos_multi_sigma_from_logprob(lp, 2);
    if (!(s_lp > 0.8f && s_lp <= 1.0f)) return -1;

    /* 2. σ_entropy: two token rows, one peaky, one uniform. */
    float peaky[4]   = { 0.0f, -5.0f, -5.0f, -5.0f };
    float uniform[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    const float *rows[2] = { peaky, uniform };
    int tn[2] = { 4, 4 };
    float s_en = cos_multi_sigma_entropy(rows, tn, 2);
    /* Peaky ≈ 0, uniform ≈ 1, mean ≈ 0.5 ± slack. */
    if (!(s_en > 0.3f && s_en < 0.7f)) return -1;

    /* 3. σ_perplexity on the same lp array. mean_lp = -1.005,
     *    exp = 0.366, σ_ppl = 0.634 ± 0.01. */
    float s_pp = cos_multi_sigma_perplexity(lp, 2);
    if (!(s_pp > 0.55f && s_pp < 0.75f)) return -1;

    /* 4. σ_consistency.
     *    Identical texts → 0.0; disjoint texts → 1.0. */
    const char *same[3] = {
        "the quick brown fox",
        "the quick brown fox",
        "the quick brown fox",
    };
    const char *diff[3] = {
        "alpha beta gamma",
        "delta epsilon zeta",
        "eta theta iota",
    };
    float s_cs_same = cos_multi_sigma_consistency(same, 3);
    float s_cs_diff = cos_multi_sigma_consistency(diff, 3);
    if (!(s_cs_same < 1e-3f))           return -1;
    if (!(s_cs_diff > 0.9f))             return -1;

    /* 5. Ensemble: combined score stays in [0,1]. */
    cos_multi_sigma_t ens;
    if (cos_multi_sigma_combine(s_lp, s_en, s_pp, s_cs_diff, NULL, &ens) != 0)
        return -1;
    if (!(ens.sigma_combined >= 0.0f && ens.sigma_combined <= 1.0f))
        return -1;
    /* All four positive → combined must be positive. */
    if (!(ens.sigma_combined > 0.3f)) return -1;

    /* 6. Guard against bad inputs. */
    if (cos_multi_sigma_from_logprob(NULL, 2) >= 0.0f) return -1;
    if (cos_multi_sigma_entropy(NULL, tn, 2) >= 0.0f)  return -1;
    if (cos_multi_sigma_perplexity(NULL, 2) >= 0.0f)   return -1;
    if (cos_multi_sigma_consistency(NULL, 2) >= 0.0f)  return -1;
    if (cos_multi_sigma_consistency(same, 1) != 0.0f)  return -1; /* k=1 */

    return 0;
}
