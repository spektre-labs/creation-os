/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Two-component uncertainty: aleatoric (self-consistency σ_semantic per model)
 * plus epistemic (cross-model spread of those σ values).
 */
#ifndef COS_SIGMA_TOTAL_UNCERTAINTY_H
#define COS_SIGMA_TOTAL_UNCERTAINTY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float au;
    float eu;
    float total;
    float sigma_final;
    int   n_models;
    char  models_used[256];
    float sigma_per_model[8];
} cos_total_uncertainty_t;

/**
 * Per-model: cos_semantic_sigma_ex(..., n_samples=3, primary=NULL).
 * AU = mean(per-model σ); EU = sample variance of those σ (0 if n_models<2).
 * total = AU + EU (clamped to 1). sigma_final = 0.6*AU + 0.4*EU (clamped).
 */
cos_total_uncertainty_t cos_measure_total(int port, const char *prompt,
                                          const char **models, int n_models);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_TOTAL_UNCERTAINTY_H */
