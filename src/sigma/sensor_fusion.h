/*
 * σ-weighted multimodal fusion over perception results (BSC contradiction cue).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SENSOR_FUSION_H
#define COS_SIGMA_SENSOR_FUSION_H

#include "perception.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cos_fusion_result {
    char  fused_description[4096];
    float sigma_fused;
    float sigma_per_modality[3];
    int   n_modalities;
    int   dominant_modality;
    int   contradiction;
};

int cos_sensor_fusion(const struct cos_perception_result *results, int n_results,
                      struct cos_fusion_result *fused);

#ifdef CREATION_OS_ENABLE_SELF_TESTS
void cos_sensor_fusion_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_SENSOR_FUSION_H */
