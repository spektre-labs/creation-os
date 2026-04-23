/*
 * Lightweight rigid-body-ish physics for embodied digital twin (AABB + Euler).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PHYSICS_MODEL_H
#define COS_SIGMA_PHYSICS_MODEL_H

#include "embodiment_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_physics_object {
    uint64_t id;
    float    position[3];
    float    size[3];
    float    mass;
    int      graspable;
    int      static_object;
};

struct cos_physics_world {
    struct cos_physics_object objects[128];
    int                     n_objects;
    float                   gravity;
    float                   friction;

    struct cos_physical_state agent;
    float                     agent_extent[3];
};

int cos_physics_init(struct cos_physics_world *w);

int cos_physics_add_object(struct cos_physics_world *w,
                           const struct cos_physics_object *obj);

int cos_physics_simulate_step(struct cos_physics_world *w, float dt);

int cos_physics_predict_action(struct cos_physics_world *w,
                               const struct cos_action *action,
                               struct cos_physical_state *result);

int cos_physics_collision_check(const struct cos_physics_world *w,
                                const float position[3],
                                const float size[3]);

float cos_physics_prediction_sigma(int collision_flag, float motion_speed);

int cos_physics_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_PHYSICS_MODEL_H */
