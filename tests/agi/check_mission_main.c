/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "../../src/sigma/mission.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

static int approx(float a, float b, float tol)
{
    return fabsf(a - b) <= tol;
}

int main(void)
{
    if (cos_mission_self_test() != 0)
        return 1;

    struct cos_mission m;
    memset(&m, 0, sizeof(m));
    snprintf(m.mission_id, sizeof m.mission_id, "test-id");
    m.n_steps = 5;
    for (int i = 0; i < 5; ++i) {
        m.steps[i].status      = COS_MISSION_STEP_DONE;
        m.steps[i].sigma       = 0.1f * (float)(i + 1);
        m.steps[i].step_id     = i;
        m.steps[i].description[0] = 'x';
    }
    float tr = cos_mission_sigma_trend(&m);
    if (tr <= 0.f)
        return 2;

    memset(&m, 0, sizeof(m));
    m.n_steps = 4;
    for (int i = 0; i < 4; ++i) {
        m.steps[i].status = COS_MISSION_STEP_DONE;
        m.steps[i].sigma  = 0.5f;
        m.steps[i].step_id = i;
    }
    if (!approx(cos_mission_sigma_trend(&m), 0.f, 1e-5f))
        return 3;

    if (cos_mission_create("", &m) == 0)
        return 4;

    if (cos_mission_pause(&m, "test") != 0 || !m.paused)
        return 5;
    if (cos_mission_resume(&m) != 0 || m.paused)
        return 6;

    struct cos_mission mz;
    memset(&mz, 0, sizeof(mz));
    mz.n_steps       = 1;
    mz.current_step  = 0;
    snprintf(mz.mission_id, sizeof mz.mission_id, "no-checkpoints");
    {
        int rrb = cos_mission_rollback(&mz);
        if (rrb != -2 && rrb != -1)
            return 7;
    }

    const char *prev = getenv("COS_MISSION_OFFLINE");
    setenv("COS_MISSION_OFFLINE", "1", 1);
    struct cos_mission ck;
    if (cos_mission_create("checkpoint hash probe", &ck) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prev, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return 8;
    }
    if (cos_mission_checkpoint_save(&ck, 0) != 0) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prev, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return 9;
    }
    if (strlen(ck.steps[0].checkpoint_hash) != 64) {
        if (prev)
            setenv("COS_MISSION_OFFLINE", prev, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return 10;
    }
    char *rep = cos_mission_report(&ck);
    if (!rep || strstr(rep, ck.mission_id) == NULL) {
        free(rep);
        if (prev)
            setenv("COS_MISSION_OFFLINE", prev, 1);
        else
            unsetenv("COS_MISSION_OFFLINE");
        return 11;
    }
    free(rep);
    if (prev)
        setenv("COS_MISSION_OFFLINE", prev, 1);
    else
        unsetenv("COS_MISSION_OFFLINE");

    return 0;
}
