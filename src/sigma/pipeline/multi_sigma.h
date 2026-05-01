/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-pipeline: multi-σ ensemble (SCI-5).
 *
 * One scalar σ is not enough.  Four orthogonal signals, weighted into
 * a single ensemble score:
 *
 *   σ_logprob      max per-token (1 − p_selected).  The SCI-1 signal.
 *   σ_entropy      mean per-token Shannon entropy over top-k probs.
 *                  Bigger when the model is "diffuse" even if it
 *                  still picks a high-p token.
 *   σ_perplexity   1 − exp(mean_logprob) on the accepted tokens,
 *                  mapped into [0,1].  Sequence-level smoothing.
 *   σ_consistency  1 − mean pairwise Jaccard on word-bags of K
 *                  regenerations of the same prompt.  Separates
 *                  epistemic (high consistency → aleatoric) from
 *                  aleatoric uncertainty (low consistency → model
 *                  is genuinely unsure).
 *
 *   σ_combined     w1·σ_logprob + w2·σ_entropy
 *                + w3·σ_perplexity + w4·σ_consistency
 *
 * All four components are in [0,1]; weights are normalised to sum
 * to 1 so σ_combined ∈ [0,1].  Defaults favour σ_logprob (backwards-
 * compatible with SCI-1) and σ_consistency (the only truly epistemic
 * signal we have without a second model):
 *
 *         w = (0.50, 0.20, 0.10, 0.20).
 *
 * Conformal calibration from SCI-1 trivially extends to any scalar
 * score, so `cos_multi_sigma_combine(...)` can feed directly into
 * `cos_conformal_calibrate`.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MULTI_SIGMA_H
#define COS_SIGMA_MULTI_SIGMA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma_logprob;
    float sigma_entropy;
    float sigma_perplexity;
    float sigma_consistency;
    float sigma_combined;
} cos_multi_sigma_t;

typedef struct {
    float w_logprob;     /* default 0.50 */
    float w_entropy;     /* default 0.20 */
    float w_perplexity;  /* default 0.10 */
    float w_consistency; /* default 0.20 */
} cos_multi_sigma_weights_t;

/* Default weights (sum to 1.0). */
cos_multi_sigma_weights_t cos_multi_sigma_default_weights(void);

/* --------- individual components --------- */

/* σ_logprob: max_t (1 − exp(logprob_selected_t)).
 * Returns -1.0 on bad input (NULL or n_tokens <= 0). */
float cos_multi_sigma_from_logprob(const float *logprob_selected,
                                   int n_tokens);

/* σ_entropy: mean per-token normalised Shannon entropy.
 *
 * top_logprobs_row[t] points to an array of `top_n[t]` log-probs for
 * the t-th token's top-k candidates.  Entropy is computed over the
 * softmax of those log-probs (one row at a time, so the caller is
 * free to pass already-normalised probs by setting all log-probs to
 * log(p)).  Normalised per-token entropy is H(row)/log(top_n[t]) so
 * the output lands in [0,1].  The return value is the arithmetic
 * mean across tokens.
 *
 * Returns -1.0 on bad input. */
float cos_multi_sigma_entropy(const float * const *top_logprobs_row,
                              const int *top_n,
                              int n_tokens);

/* σ_perplexity: clipped sequence perplexity.
 *
 *     mean_lp = (1/N) Σ logprob_selected
 *     σ_ppl   = 1 − exp(mean_lp)   clipped to [0,1]
 *
 * Returns -1.0 on bad input. */
float cos_multi_sigma_perplexity(const float *logprob_selected,
                                 int n_tokens);

/* σ_consistency: 1 − mean_pairwise_jaccard(word-bags of K texts).
 *
 * K ≥ 2.  Each text is tokenised on ASCII whitespace/punctuation
 * (cheap stopword-free bag).  Jaccard on the (distinct) word sets.
 *
 * K = 1 is treated as "no signal" → returns 0.0 (consistent with a
 * single observation).  Returns -1.0 on NULL input. */
float cos_multi_sigma_consistency(const char *const *regen_texts,
                                  int k);

/* --------- ensemble --------- */

/* Combine four components.  Weights are renormalised so w sums to 1.0
 * (defensively); components are clamped to [0,1].  NULL `weights`
 * means "use defaults".  Always returns 0.  Caller owns `out`. */
int cos_multi_sigma_combine(float sigma_logprob,
                            float sigma_entropy,
                            float sigma_perplexity,
                            float sigma_consistency,
                            const cos_multi_sigma_weights_t *weights,
                            cos_multi_sigma_t *out);

/* Self-test: numerically checks each component on a hand-built case
 * and verifies that σ_combined with default weights falls inside
 * [0,1].  Returns 0 on success. */
int cos_multi_sigma_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_MULTI_SIGMA_H */
