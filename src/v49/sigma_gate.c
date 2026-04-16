/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma_gate.h"

#include <math.h>

v49_action_t v49_sigma_gate_calibrated_epistemic(float calibrated_epistemic, float threshold)
{
    if (!isfinite(calibrated_epistemic) || !isfinite(threshold)) {
        return V49_ACTION_ABSTAIN;
    }
    if (calibrated_epistemic > threshold) {
        return V49_ACTION_ABSTAIN;
    }
    return V49_ACTION_EMIT;
}
