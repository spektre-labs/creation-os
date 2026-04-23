/*
 * Harness for embodiment layer (eight API checks + embedded self-test).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../../src/sigma/embodiment.h"
#include "../../src/sigma/sovereign_limits.h"

#include <stddef.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_embodiment          e;
    struct cos_action              act;
    struct cos_physical_state      pred, alt;
    struct cos_perception_result   pr;
    struct cos_sovereign_limits    L;
    float                          gap;

    cos_sovereign_default_limits(&L);
    L.require_human_above = 1.0f;
    L.max_autonomy_level  = 1.0f;
    if (cos_sovereign_init(&L) != 0)
        return 21;

    if (cos_embodiment_init(NULL, 1) != -1)
        return 22;
    if (cos_embodiment_init(&e, 1) != 0)
        return 23;

    memset(&pr, 0, sizeof pr);
    pr.sigma = 0.6f;
    snprintf(pr.description, sizeof pr.description, "cli check object");
    if (cos_embodiment_perceive(&e, &pr) != 0)
        return 24;

    if (cos_embodiment_plan_action(&e, "move forward 1m", &act) != 0)
        return 25;

    {
        int src = cos_embodiment_simulate(&e, &act, &pred);
        if (src != 0 && src != -2)
            return 26;
    }

    alt = pred;
    alt.position[1] += 0.12f;
    gap = cos_embodiment_sim_to_real_gap(&pred, &alt);
    if (gap < 0.f || gap > 1.f)
        return 27;

    if (cos_embodiment_update_world_model(&e, gap) != 0)
        return 28;

    if (cos_embodiment_execute(&e, &act) != 0)
        return 29;

    return cos_embodiment_self_test() != 0 ? 30 : 0;
}
