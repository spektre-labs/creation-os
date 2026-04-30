/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * Liquid σ-neuron: Q16.16 leaky state + per-neuron cognitive gate (Python ``sigma_gate_core``
 * semantics). Does **not** include ``sigma_gate.h`` — this tree keeps a single Q16 mirror
 * here so ``src/sigma`` builds even when the Python header path is a shim.
 */
#ifndef CREATION_OS_LIQUID_SIGMA_NEURON_H
#define CREATION_OS_LIQUID_SIGMA_NEURON_H

#include <stdint.h>

typedef struct cos_sigma_gate_state {
    int32_t sigma;
    int32_t d_sigma;
    int32_t k_eff;
} cos_sigma_gate_state_t;

typedef struct liquid_sigma_neuron {
    int32_t state; /* Q16.16 */
    int32_t tau;   /* Q0.16 time constant in (0, COS_Q16] */
    int32_t pad;   /* reserved / alignment */
    cos_sigma_gate_state_t gate;
} liquid_sigma_neuron_t;

#define COS_LIQUID_Q16 ((int32_t)65536)

/** ``tau_q16`` e.g. ``COS_LIQUID_Q16 / 4`` for a quarter-step leak toward ``input``. */
void liquid_sigma_neuron_init(liquid_sigma_neuron_t *n, int32_t tau_q16);

/**
 * Closed-form style leak toward ``input_q16``; maps |error| to σ and runs the gate.
 * Returns ``0`` on ABSTAIN (silent), else updated ``state``.
 */
int32_t liquid_sigma_neuron_update(liquid_sigma_neuron_t *n, int32_t input_q16);

#endif /* CREATION_OS_LIQUID_SIGMA_NEURON_H */
