/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "progressive_distill.h"

#include <math.h>

const v43_distill_stage_t v43_distill_stages[4] = {
    {1, 0.0f, 0.2f, 1e-4f, 0},
    {2, 0.2f, 0.5f, 5e-5f, 0},
    {3, 0.5f, 0.7f, 1e-5f, 0},
    {4, 0.7f, 1.0f, 0.0f, 0},
};

static float v43_clamp01(float x)
{
    if (x < 0.0f) {
        return 0.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}

int v43_stage_index_for_teacher_epistemic(float epistemic)
{
    float e = v43_clamp01(epistemic);
    if (e < 0.2f) {
        return 0;
    }
    if (e < 0.5f) {
        return 1;
    }
    if (e < 0.7f) {
        return 2;
    }
    return 3;
}

const v43_distill_stage_t *v43_stage_for_teacher_epistemic(float epistemic)
{
    int i = v43_stage_index_for_teacher_epistemic(epistemic);
    return &v43_distill_stages[i];
}
