/*
 * Shared physical pose / action types for embodiment + physics twin.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_EMBODIMENT_TYPES_H
#define COS_SIGMA_EMBODIMENT_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum cos_action_type_enum {
    COS_ACTION_MOVE = 0,
    COS_ACTION_GRASP = 1,
    COS_ACTION_RELEASE = 2,
    COS_ACTION_LOOK = 3,
    COS_ACTION_SPEAK = 4,
    COS_ACTION_WAIT = 5,
};

struct cos_physical_state {
    float    position[3];
    float    orientation[4];
    float    velocity[3];
    float    battery;
    float    temperature;
    int      sensors_active;
    int      actuators_active;
    int64_t  timestamp_ms;
};

struct cos_action {
    int   type;
    float params[8];
    float sigma;
    float risk;
    int   reversible;
    char  description[256];
};

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_EMBODIMENT_TYPES_H */
