/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "decompose_v39.h"

#include <math.h>
#include <string.h>

float sigma_hardware_estimate(float nominal_output, float measured_output, float temperature_celsius)
{
    const float eps = 1e-6f;
    float denom = fabsf(nominal_output) + eps;
    float drift = fabsf(nominal_output - measured_output) / denom;
    float thermal = 0.001f * (temperature_celsius / 300.0f);
    if (thermal < 0.0f) {
        thermal = 0.0f;
    }
    return drift + thermal;
}

void sigma_full_from_decomposed(const sigma_decomposed_t *sw, float hardware, sigma_full_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (!sw) {
        return;
    }
    out->aleatoric = sw->aleatoric;
    out->epistemic = sw->epistemic;
    out->hardware = hardware;
    out->total = sw->aleatoric + sw->epistemic + hardware;
}
