/*
 * Composite sustainability score from energy accounting + operational ratios.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_GREEN_SCORE_H
#define COS_SIGMA_GREEN_SCORE_H

#include "energy_accounting.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cos_green_score {
    float energy_efficiency;
    float cache_hit_ratio;
    float abstain_ratio;
    float spike_suppress_ratio;
    float locality_ratio;

    float green_score;
    char  grade;

    float vs_gpt5_ratio;
    float vs_cloud_ratio;
    float co2_saved_lifetime_kg;
};

struct cos_green_score cos_green_calculate(
    const struct cos_energy_measurement *lifetime);

void cos_green_print(const struct cos_green_score *g);

char *cos_green_to_json(const struct cos_green_score *g);

int cos_green_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_GREEN_SCORE_H */
