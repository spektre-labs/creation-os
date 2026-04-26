/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#ifndef COS_SIGMA_CROSS_MODEL_H
#define COS_SIGMA_CROSS_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char  model[64];
    float sigma;
    char  action[16];
    char  response[4096];
} cos_model_result_t;

/** Query each model (HTTP); fills results[0..n_models-1]. Returns 0. */
int cos_cross_model_query(int port, const char *prompt, const char **models,
                          int n_models, cos_model_result_t *results);

int cos_select_best_model(const cos_model_result_t *results, int n_models);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_CROSS_MODEL_H */
