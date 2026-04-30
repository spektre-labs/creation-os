/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * 14-phase Ω bundle: one Q16 cognitive interrupt state per phase plus a master lane.
 * Mirrors ``python/cos/sigma_gate_core.py`` (same layout as ``cos_sigma_gate_state_t`` in
 * ``liquid_neuron.h``). Does **not** include ``python/cos/sigma_gate.h``.
 */
#ifndef CREATION_OS_OMEGA_PHASE_GATES_H
#define CREATION_OS_OMEGA_PHASE_GATES_H

#include "liquid_neuron.h"
#include <stdint.h>

typedef enum {
    COS_OMEGA_PERCEIVE = 0,
    COS_OMEGA_PREDICT,
    COS_OMEGA_REMEMBER,
    COS_OMEGA_THINK,
    COS_OMEGA_GATE,
    COS_OMEGA_SAFETY,
    COS_OMEGA_SIMULATE,
    COS_OMEGA_ACT,
    COS_OMEGA_PROVE,
    COS_OMEGA_LEARN,
    COS_OMEGA_REFLECT,
    COS_OMEGA_CONSOLIDATE,
    COS_OMEGA_WATCHDOG,
    COS_OMEGA_CONTINUE,
    COS_OMEGA_N_PHASES = 14
} cos_omega_phase_id_t;

typedef struct cos_omega_phase_bundle {
    cos_sigma_gate_state_t master;
    cos_sigma_gate_state_t phases[COS_OMEGA_N_PHASES];
    int32_t                total_sigma_q16;
    uint32_t               turn;
} cos_omega_phase_bundle_t;

void cos_omega_phase_bundle_init(cos_omega_phase_bundle_t *b);

/**
 * Update ``phases[phase]`` with ``phase_sigma_q16``, blend into ``total_sigma_q16``,
 * refresh ``master``, and return the **per-phase** verdict (0 ACCEPT, 1 RETHINK, 2 ABSTAIN).
 */
int cos_omega_phase_step(cos_omega_phase_bundle_t *b, cos_omega_phase_id_t phase,
                         int32_t phase_sigma_q16, int32_t k_raw_q16);

#endif /* CREATION_OS_OMEGA_PHASE_GATES_H */
