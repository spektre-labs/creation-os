/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Two-component uncertainty: aleatoric (self-consistency σ_semantic)
 * plus epistemic (cross-model spread of σ_semantic).
 */
#ifndef COS_TOTAL_UNCERTAINTY_H
#define COS_TOTAL_UNCERTAINTY_H

#ifdef __cplusplus
extern "C" {
#endif

#define COS_TU_MAX_MODELS 8

typedef struct {
    float au; /* mean σ_semantic across models */
    float eu; /* population variance of σ_semantic across models */
    float total;        /* au + eu */
    float sigma_final; /* 0.6*au + 0.4*eu (tunable via env) */
    int   n_models;
    char  models_used[256];
    float sigma_per_model[COS_TU_MAX_MODELS];
    char  model_name[COS_TU_MAX_MODELS][64];
} cos_total_uncertainty_t;

/**
 * For each model, runs cos_semantic_sigma_ex (3-sample self-consistency).
 * n_models==1 → eu=0, total=au. n_models<1 → zeroed struct.
 */
cos_total_uncertainty_t cos_measure_total(int port, const char *prompt,
                                          const char **models, int n_models);

#ifdef __cplusplus
}
#endif
#endif /* COS_TOTAL_UNCERTAINTY_H */
