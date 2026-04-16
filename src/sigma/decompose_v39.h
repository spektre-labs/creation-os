/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v39: extend σ decomposition with a hardware / substrate noise term (lab).
 */
#ifndef CREATION_OS_SIGMA_DECOMPOSE_V39_H
#define CREATION_OS_SIGMA_DECOMPOSE_V39_H

#include "decompose.h"

typedef struct {
    float total;
    float aleatoric;
    float epistemic;
    float hardware;
} sigma_full_t;

/** Map nominal vs measured column output + temperature into a σ_hardware scalar (unitless, >= 0). */
float sigma_hardware_estimate(float nominal_output, float measured_output, float temperature_celsius);

/** Combine software Dirichlet decomposition with hardware σ: σ_total = σ_aleatoric + σ_epistemic + σ_hardware. */
void sigma_full_from_decomposed(const sigma_decomposed_t *sw, float hardware, sigma_full_t *out);

#endif /* CREATION_OS_SIGMA_DECOMPOSE_V39_H */
