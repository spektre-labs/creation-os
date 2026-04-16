/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "doubt_reward.h"

#include <math.h>

float v45_doubt_reward(const char *response, int is_correct, float sigma_epistemic, float self_reported_confidence)
{
    (void)response;
    if (is_correct == 1 && sigma_epistemic < 0.3f) {
        return 1.0f;
    }
    if (is_correct == 0 && sigma_epistemic > 0.7f) {
        return 0.5f;
    }
    if (is_correct == 0 && sigma_epistemic < 0.2f) {
        return -1.0f;
    }
    if (is_correct == 1 && sigma_epistemic > 0.7f) {
        return 0.3f;
    }
    float cal_bonus = 1.0f - fabsf(self_reported_confidence - (1.0f - sigma_epistemic));
    if (cal_bonus < 0.0f) {
        cal_bonus = 0.0f;
    }
    if (cal_bonus > 1.0f) {
        cal_bonus = 1.0f;
    }
    return 0.2f * cal_bonus;
}
