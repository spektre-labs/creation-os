/*
 * Living weights — fast-weight sketch + anti-forgetting probes (σ).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_LIVING_WEIGHTS_H
#define COS_SIGMA_LIVING_WEIGHTS_H

#include "engram_episodic.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_living_weights {
    float   weight_deltas[512];
    float   sigma_before_update;
    float   sigma_after_update;
    int     updates_applied;
    int     updates_reverted;
    float   improvement_rate;
    int64_t last_update_ms;
};

int cos_living_weights_init(struct cos_living_weights *lw);

int cos_living_weights_adapt(struct cos_living_weights *lw,
                             const struct cos_engram_episode *recent, int n);

int cos_living_weights_test_forgetting(struct cos_living_weights *lw);

int cos_living_weights_apply(struct cos_living_weights *lw);

int cos_living_weights_revert(struct cos_living_weights *lw);

int cos_living_weights_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_LIVING_WEIGHTS_H */
