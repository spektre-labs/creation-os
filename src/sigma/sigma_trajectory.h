/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#ifndef COS_SIGMA_TRAJECTORY_H
#define COS_SIGMA_TRAJECTORY_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma_per_step[64];
    int   n_steps;
    float sigma_final;
    float sigma_max;
    float sigma_mean;
    float sigma_slope;
    int   spike_count;
} cos_sigma_trajectory_t;

cos_sigma_trajectory_t cos_measure_trajectory(int port, const char *prompt,
                                              const char *model);

const char *cos_trajectory_action(const cos_sigma_trajectory_t *t);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_TRAJECTORY_H */
