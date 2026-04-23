/*
 * Embodied agent loop — perception, twin simulation, sovereign-gated actuation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_EMBODIMENT_H
#define COS_SIGMA_EMBODIMENT_H

#include "embodiment_types.h"
#include "physics_model.h"
#include "perception.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_embodiment {
    struct cos_physical_state state;
    struct cos_action         pending_action;
    struct cos_action         history[256];
    int                       n_history;
    float                     sigma_physical;
    int                       simulation_mode;

    struct cos_physics_world  physics;
    float                     sigma_sim_to_real;
    float                     tau_physical;

    struct cos_physical_state last_predicted_state;
    float                     last_sim_sigma;
};

int cos_embodiment_init(struct cos_embodiment *e, int simulation_mode);

int cos_embodiment_perceive(struct cos_embodiment *e,
                            const struct cos_perception_result *perception);

int cos_embodiment_plan_action(struct cos_embodiment *e,
                               const char *goal,
                               struct cos_action *action);

int cos_embodiment_simulate(struct cos_embodiment *e,
                            const struct cos_action *action,
                            struct cos_physical_state *predicted);

int cos_embodiment_execute(struct cos_embodiment *e,
                           const struct cos_action *action);

float cos_embodiment_sim_to_real_gap(
    const struct cos_physical_state *predicted,
    const struct cos_physical_state *actual);

int cos_embodiment_update_world_model(struct cos_embodiment *e,
                                      float sim_to_real_gap);

void cos_embodiment_print_state(FILE *fp, const struct cos_embodiment *e);

void cos_embodiment_print_history(FILE *fp, const struct cos_embodiment *e);

int cos_embodiment_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_EMBODIMENT_H */
