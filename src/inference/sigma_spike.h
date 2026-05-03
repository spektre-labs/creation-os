/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * v154 σ-spike inference — Leaky Integrate-and-Fire lanes validated by the same Q16
 * cognitive gate shape as ``src/sigma/sigma_spike.c`` (uses ``cos_sigma_gate_state_t``
 * from ``liquid_neuron.h`` only; does **not** include ``python/cos/sigma_gate.h``).
 */
#ifndef CREATION_OS_INFERENCE_SIGMA_SPIKE_H
#define CREATION_OS_INFERENCE_SIGMA_SPIKE_H

#include "liquid_neuron.h"

#include <stdint.h>

#define COS_INFER_SPIKE_N_PHASES 14
#define COS_INFER_SPIKE_Q16 ((int32_t)65536)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_infer_sigma_lif_neuron {
    int32_t                membrane;
    int32_t                threshold;
    int32_t                leak;
    int32_t                refractory;
    int32_t                refrac_counter;
    uint32_t               spikes_fired;
    uint32_t               spikes_suppressed;
    cos_sigma_gate_state_t gate;
} cos_infer_sigma_lif_neuron_t;

typedef struct cos_infer_sigma_spike_omega {
    cos_infer_sigma_lif_neuron_t neurons[COS_INFER_SPIKE_N_PHASES];
    uint32_t                       active_phases;
    uint32_t                       chain_passes;
} cos_infer_sigma_spike_omega_t;

void cos_infer_sigma_lif_init(cos_infer_sigma_lif_neuron_t *n, int32_t threshold, int32_t leak,
                              int32_t refractory);

/** Returns 1 if an ACCEPT-validated spike propagates; 0 if suppressed or sub-threshold. */
int cos_infer_sigma_lif_step(cos_infer_sigma_lif_neuron_t *n, int32_t input, int32_t k_raw_q16);

void cos_infer_sigma_lif_idle(cos_infer_sigma_lif_neuron_t *n);

void cos_infer_sigma_spike_omega_init(cos_infer_sigma_spike_omega_t *o, int32_t threshold,
                                      int32_t leak, int32_t refractory);

/**
 * Event-driven chain: phase i+1 integrates only if phase i produced an ACCEPT spike.
 * ``input0`` is Q0 integer drive into phase 0 (e.g. hashed prompt energy).
 */
void cos_infer_sigma_spike_omega_forward(cos_infer_sigma_spike_omega_t *o, int32_t input0,
                                         int32_t k_raw_q16);

/** Q16.16 ratio suppressed / (fired + suppressed) aggregated over all lanes; 0 if no events. */
int32_t cos_infer_sigma_spike_omega_sparsity_q16(const cos_infer_sigma_spike_omega_t *o);

/**
 * Lab-only float in [0,1] — not used on hot paths in silicon builds.
 * sparsity ≈ suppressed / total events; target suites report >0.85 for idle-heavy prompts.
 */
float cos_infer_sigma_spike_omega_sparsity_f32(const cos_infer_sigma_spike_omega_t *o);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_INFERENCE_SIGMA_SPIKE_H */
