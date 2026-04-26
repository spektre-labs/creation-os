/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Cross-model semantic disagreement σ: three Ollama models, same prompt,
 * Jaccard on first-sentence extracts.  σ ≈ 1 − mean pairwise similarity.
 * Disable parallel fetches with COS_CROSS_MODEL_SIGMA_PARALLEL=0.
 */
#ifndef COS_CROSS_MODEL_SIGMA_H
#define COS_CROSS_MODEL_SIGMA_H

#ifdef __cplusplus
extern "C" {
#endif

/** Returns σ in [0,1]; on transport failure returns ~0.85. */
float cos_cross_model_sigma(int port, const char *prompt,
                            const char *system_prompt);

/**
 * Epistemic disagreement across models: spread(max σ − min σ) on per-model
 * semantic-entropy σ. (Same numeric scale as the cross-model CLI examples.)
 * Returns 0 if n_models < 2.
 */
float cos_cross_model_sigma_eu_from_sigmas(const float *sigmas, int n_models);

/**
 * For each model, run semantic-entropy σ; returns EU = max(σ_i) − min(σ_i).
 * On failure returns 1.0f (conservative).
 */
float cos_cross_model_eu(int port, const char *prompt, const char **models,
                         int n_models);

#ifdef __cplusplus
}
#endif

#endif /* COS_CROSS_MODEL_SIGMA_H */
