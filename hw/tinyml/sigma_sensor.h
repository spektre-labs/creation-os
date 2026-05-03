/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * σ from scalar sensor readings → same Q16 σ-gate as `sigma_gate_tiny.h`.
 */
#ifndef SIGMA_SENSOR_H
#define SIGMA_SENSOR_H

#include "sigma_gate_tiny.h"
#include <stdint.h>

typedef struct {
    sigma_state_t gate;
    int32_t       baseline;
    int32_t       tolerance;
    uint32_t      anomalies;
} sigma_sensor_t;

void sigma_sensor_init(sigma_sensor_t *s, int32_t baseline, int32_t tolerance);

/* Maps |reading - baseline| / tolerance to Q16 σ, then runs σ_update + σ_gate. */
sigma_verdict_t sigma_sensor_step(sigma_sensor_t *s, int32_t reading);

#endif /* SIGMA_SENSOR_H */
