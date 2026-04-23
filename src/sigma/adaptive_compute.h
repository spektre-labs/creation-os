/*
 * Sigma-guided adaptive compute allocation (budget per query).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ADAPTIVE_COMPUTE_H
#define COS_SIGMA_ADAPTIVE_COMPUTE_H

#include "spike_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cos_compute_budget {
    int   kernels_to_run;
    int   max_rethinks;
    int   enable_ttt;
    int   enable_search;
    int   enable_multimodal;
    float time_budget_ms;
    float energy_budget_mj;
};

struct cos_compute_budget cos_allocate_compute(
    float speculative_sigma,
    const struct cos_spike_engine *spikes);

int cos_allocate_compute_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_ADAPTIVE_COMPUTE_H */
