/*
 * σ-Diagnostic — explain WHY σ is what it is.
 *
 * Every other primitive in the pipeline reports a single scalar
 * σ ∈ [0, 1].  σ-Diagnostic decomposes that scalar back into the
 * three properties of the next-token distribution that drove it:
 *
 *   - factor_entropy  : how spread out the distribution is, normalised
 *                       to [0, 1] by ln(vocab_size).
 *   - factor_gap      : 1 − (p_top1 − p_top2).  Tight competition
 *                       between the top two candidates → close to 1.
 *   - factor_maxprob  : 1 − p_top1.  Low absolute confidence in
 *                       the chosen token → close to 1.
 *
 * The diagnostic σ is a uniform average of the three:
 *
 *     σ_diag = (factor_entropy + factor_gap + factor_maxprob) / 3
 *
 * Each factor lives in [0, 1] so σ_diag also lives in [0, 1].  In
 * the "model knows the answer" regime the entropy is small, the
 * gap is large (so 1 − gap is small) and p_top1 is high — all
 * three factors collapse to ≈ 0.  In the "three-way tie" regime
 * everything climbs together and σ_diag approaches 1.
 *
 * The primitive also reports
 *   - top-3 (idx, prob) pairs sorted by prob descending,
 *   - effective_k = round(exp(entropy)) — the number of tokens the
 *     model is "really" choosing among, and
 *   - a counterfactual: σ_diag if the top-1 probability were lifted
 *     to a target value (default 0.90), with the rest shrunk
 *     proportionally.  This is how the chat layer can answer
 *     "what if we provided more context?" without re-running the
 *     model.
 *
 * Contracts (v0):
 *   1. NULL logprobs / vocab_size ≤ 0 → diagnostic with σ = 1,
 *      empty top-k, factor_* = 1.
 *   2. ``logprobs`` are natural-log probabilities, not raw logits.
 *      The primitive softmax-normalises them before computing
 *      derived quantities (so callers can pass un-normalised log-
 *      probs from a logit head safely).
 *   3. NaN / inf in logprobs are dropped (treated as -inf), never
 *      poison the result.
 *   4. Counterfactual_target ∉ (0, 1) is clamped into (0, 1).
 *   5. Self-test pins all three regimes (sharp, two-way, three-way
 *      tie) plus the counterfactual delta on a known input.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_DIAGNOSTIC_H
#define COS_SIGMA_DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SIGMA_DIAG_TOPK 3

typedef struct {
    /* Headline. */
    float sigma;

    /* Decomposition. */
    float factor_entropy;
    float factor_gap;
    float factor_maxprob;

    /* Distribution shape. */
    float entropy;          /* in nats */
    int   effective_k;      /* round(exp(entropy)) */
    float max_prob;         /* p_top1 */
    float gap_top12;        /* p_top1 − p_top2 (≥ 0)  */

    /* Top-3 (idx, prob) pairs sorted descending by prob. */
    int   top_idx [COS_SIGMA_DIAG_TOPK];
    float top_prob[COS_SIGMA_DIAG_TOPK];

    /* Counterfactual: σ if p_top1 were lifted to ``cf_target``
     * (default 0.90) with the remaining mass renormalised. */
    float cf_target;
    float cf_sigma;
} cos_sigma_diagnostic_t;

cos_sigma_diagnostic_t
cos_sigma_diagnostic_explain(const float *logprobs, int vocab_size,
                             float cf_target);

int   cos_sigma_diagnostic_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_DIAGNOSTIC_H */
