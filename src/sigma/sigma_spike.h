/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * σ-spike — event-driven σ gate (Q16 mirror of ``omega_phase_gates`` / Python ``sigma_gate_core``).
 * Fire full ``gate_update`` only when |Δinput| ≥ threshold; otherwise hold previous verdict.
 *
 * Includes ``liquid_neuron.h`` types only — does **not** include or modify ``python/cos/sigma_gate.h``.
 */
#ifndef CREATION_OS_SIGMA_SPIKE_H
#define CREATION_OS_SIGMA_SPIKE_H

#include "liquid_neuron.h"
#include "omega_phase_gates.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_sigma_spike_lane {
    cos_sigma_gate_state_t gate;
    int32_t                last_input;
    int32_t                spike_threshold;
    uint32_t               spikes_fired;
    uint32_t               spikes_suppressed;
    int                    last_verdict; /* 0 ACCEPT, 1 RETHINK, 2 ABSTAIN */
} cos_sigma_spike_lane_t;

typedef struct cos_sigma_spike_omega_bundle {
    cos_sigma_spike_lane_t lanes[COS_OMEGA_N_PHASES];
} cos_sigma_spike_omega_bundle_t;

#define COS_SIGMA_SPIKE_Q16_SCALE   ((int32_t)65536)
#define COS_SIGMA_SPIKE_FLOAT_TO_Q16(x) ((int32_t)((double)(x) * (double)COS_SIGMA_SPIKE_Q16_SCALE))

/** ``threshold_q16`` in same units as ``input_q16`` (typically σ probe in Q16). */
void cos_sigma_spike_lane_init(cos_sigma_spike_lane_t *s, int32_t threshold_q16);

/**
 * On small |Δinput|, increment ``spikes_suppressed`` and return ``last_verdict``.
 * Else run ``gate_update`` + verdict, refresh ``last_input`` / ``last_verdict``.
 */
int cos_sigma_spike_lane_step(cos_sigma_spike_lane_t *s, int32_t input_q16, int32_t k_raw_q16);

/** Q16.16 ratio ``suppressed / (fired + suppressed)``; 0 if no events yet. */
int32_t cos_sigma_spike_lane_sparsity_q16(const cos_sigma_spike_lane_t *s);

void cos_sigma_spike_omega_init(cos_sigma_spike_omega_bundle_t *b, int32_t threshold_q16);

/** One Ω turn: one probe σ per phase lane. */
void cos_sigma_spike_omega_step(cos_sigma_spike_omega_bundle_t *b, const int32_t *phase_sigma_q16,
                                 int32_t k_raw_q16);

/** Mean sparsity across lanes, Q16.16. */
int32_t cos_sigma_spike_omega_sparsity_q16(const cos_sigma_spike_omega_bundle_t *b);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_SIGMA_SPIKE_H */
