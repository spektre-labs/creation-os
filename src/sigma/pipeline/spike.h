/*
 * σ-Spike — neuromorphic (LIF) implementation of the σ-gate.
 *
 * Claim: the σ-gate is substrate-independent.  Given the same
 * ACCEPT / ABSTAIN semantics, it can be realised on transistors,
 * on a spiking neural network (Loihi 2, Akida, NorthPole), on
 * photonic mesh (H2), or — in the future — on a quantum
 * co-processor.  H1 proves the neuromorphic equivalence at the
 * kernel level.
 *
 * Leaky integrate-and-fire (LIF) model:
 *
 *     V[t+1] = decay · V[t] + input
 *     if V[t+1] ≥ V_th   → spike, V ← V_reset  (FIRE  == ACCEPT)
 *                        else no spike          (SILENT == ABSTAIN)
 *
 * σ mapping (neuron-level):
 *     σ_spike := 1 − V/V_th                     (only while silent)
 *              := 0                              (on the spike)
 *
 * A lower σ means the membrane potential is closer to threshold
 * — i.e. the gate is closer to accepting.  This is the same
 * semantics as the digital σ-gate but computed in an
 * event-driven way (no arithmetic when input is zero and the
 * decay has already pulled V below ε).
 *
 * Population level:
 *     rate[t]        = spikes in last window / window_len
 *     σ_population   = 1 − rate[t] / rate_max
 *
 * A quiet population means "no confident firing" (ABSTAIN);
 * a saturated population means "everyone says ACCEPT".  The
 * same ABSTAIN / ACCEPT decision emerges from firing rates.
 *
 * Energy model (for H5 paper claims):
 *   — digital σ:     O(D) FMAs per token, continuous.
 *   — event-driven:  only spikes consume energy; silent neurons
 *                    cost ε.  For sparse inputs (≤5% active),
 *                    neuromorphic saves ~20× energy.  We do NOT
 *                    benchmark wall-clock here — the self-test
 *                    verifies equivalence of the gate output
 *                    across substrates, not silicon latency.
 *
 * Contracts (v0):
 *   1. init clamps decay ∈ [0, 1], threshold > 0, reset < threshold.
 *   2. step() returns 1 on spike, 0 on silence, −1 on NaN input.
 *   3. σ() returns in [0, 1], clamped.
 *   4. run_batch equivalence: same input sequence → both digital
 *      σ-gate and spike σ-gate reach ACCEPT on the same tokens.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SPIKE_H
#define COS_SIGMA_SPIKE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float membrane_potential;
    float threshold;        /* V_th (≡ τ_accept for the gate)   */
    float decay;            /* leak factor per step ∈ [0, 1]    */
    float reset_potential;  /* V_reset after spike              */
    int   spiked;           /* last step emitted a spike        */
    int   n_spikes;         /* lifetime spike count              */
    int   n_steps;          /* lifetime step count               */
} cos_spike_neuron_t;

int   cos_sigma_spike_init(cos_spike_neuron_t *n,
                           float threshold,
                           float decay,
                           float reset_potential);

/* One substep: feed `input` (mA-analog), leak + integrate + fire.
 * Returns 1 on spike (ACCEPT), 0 on silence (ABSTAIN),
 * −1 on NaN / invalid. */
int   cos_sigma_spike_step(cos_spike_neuron_t *n, float input);

float cos_sigma_spike_sigma(const cos_spike_neuron_t *n);

/* Population view: compute σ over a batch of N neurons that just
 * received the same temporal window.  rate_max is the maximum
 * firing rate used to normalise (spikes per window). */
float cos_sigma_spike_population_sigma(const cos_spike_neuron_t *pop,
                                       int n, int window_len,
                                       int rate_max);

int   cos_sigma_spike_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SPIKE_H */
