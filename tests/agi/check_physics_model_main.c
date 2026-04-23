/*
 * Harness for physics twin self-test (six API checks + embedded self-test).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../../src/sigma/physics_model.h"

#include <stddef.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_physics_world    w;
    struct cos_physics_object   o;
    struct cos_action             act;
    struct cos_physical_state     pred;
    float                         s;

    if (cos_physics_init(NULL) != -1)
        return 11;
    if (cos_physics_init(&w) != 0)
        return 12;
    if (cos_physics_add_object(&w, NULL) != -1)
        return 13;

    memset(&o, 0, sizeof o);
    o.static_object = 0;
    o.mass          = 1.f;
    o.size[0]       = 0.2f;
    o.size[1]       = 0.2f;
    o.size[2]       = 0.2f;
    o.position[0]   = 5.f;
    if (cos_physics_add_object(&w, &o) != 0)
        return 14;

    memset(&act, 0, sizeof act);
    act.type       = COS_ACTION_MOVE;
    act.params[0]  = 0.05f;
    if (cos_physics_predict_action(&w, &act, &pred) != 0)
        return 15;

    s = cos_physics_prediction_sigma(0, 2.0f);
    if (s < 0.f || s > 1.f)
        return 16;

    if (cos_physics_collision_check(NULL, pred.position, w.agent_extent)
        != -1)
        return 17;

    if (cos_physics_simulate_step(&w, 0.02f) != 0)
        return 18;

    return cos_physics_self_test() != 0 ? 19 : 0;
}
