/*
 * Nested update cadence for predictive σ-world (lab).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_OMEGA_CONTINUAL_LEARNING_H
#define COS_OMEGA_CONTINUAL_LEARNING_H

#include "predictive_world.h"

#ifdef __cplusplus
extern "C" {
#endif

void cos_continual_learning_tick(cos_predictive_world_t *wm, int query_count);

int cos_continual_learning_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_OMEGA_CONTINUAL_LEARNING_H */
