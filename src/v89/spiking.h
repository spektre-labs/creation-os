/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v89 — σ-Spiking (Loihi-3 style neuromorphic plane).
 *
 * ------------------------------------------------------------------
 *  What v89 is
 * ------------------------------------------------------------------
 *
 * v89 is a minimal, deterministic, integer-only event-driven
 * spiking-network plane in the style of Intel Loihi-3 (Jan 2026) —
 * the first commercial neuromorphic chip with *32-bit graded
 * spikes*.  Graded spikes bridge SNN and DNN: instead of binary
 * {0,1} spikes, a neuron may emit any int32 payload on spike,
 * letting backprop-trained features cohabit with brain-style event
 * processing.  Energy per inference on Loihi-3 is ~1000× lower than
 * GPU inference for equivalent workloads.
 *
 * Primitives (integer Q16.16 for membrane potential, int32 graded
 * spike payloads):
 *
 *   1. cos_v89_net_init      — zero-weight network, τ_mem, V_θ, τ_stdp
 *   2. cos_v89_connect       — add directed synapse i → j, weight w
 *   3. cos_v89_inject        — deliver an external graded input to
 *                              neuron j at time t.
 *   4. cos_v89_step          — advance one discrete timestep, apply
 *                              leak, fire above threshold (graded
 *                              spike payload = V - V_θ at fire), run
 *                              STDP on recent pre-post pairs.
 *   5. cos_v89_spike_count   — number of spikes emitted so far.
 *   6. cos_v89_weight        — read a synaptic weight.
 *   7. cos_v89_lif_decay     — test helper: isolated LIF decay
 *                              rule (τ_mem=16 → V ← V - V/16).
 *
 * STDP rule (integer, branchless-ish):
 *
 *     Δw_ij = + A_plus  if pre fires just before post   (causal)
 *             - A_minus if post fires just before pre   (acausal)
 *
 * A_plus, A_minus are int16 constants; the window is τ_stdp ticks.
 * Weights are clamped to a [-w_max, +w_max] range.
 *
 * ------------------------------------------------------------------
 *  Composed 29-bit branchless decision (extends v88)
 * ------------------------------------------------------------------
 *
 *     cos_v89_compose_decision(v88_composed_ok, v89_ok)
 *         = v88_composed_ok & v89_ok
 *
 * `v89_ok = 1` iff sentinel is intact, no weight has fallen outside
 * its clamp, and no membrane potential has ever exceeded the
 * overflow ceiling (|V| < V_max).
 */

#ifndef COS_V89_SPIKING_H
#define COS_V89_SPIKING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V89_SENTINEL      0x5911C042u
#define COS_V89_NUM_NEURONS   64u
#define COS_V89_MAX_SYN       512u
#define COS_V89_STDP_WINDOW   8u

typedef int32_t cos_v89_q16_t;

typedef struct {
    uint16_t      pre;
    uint16_t      post;
    int32_t       weight;           /* Q16.16, clamped [-w_max, +w_max] */
} cos_v89_syn_t;

typedef struct {
    cos_v89_q16_t V   [COS_V89_NUM_NEURONS];     /* membrane potential */
    int32_t       last_spike_t[COS_V89_NUM_NEURONS]; /* -1 if never spiked */
    int32_t       last_spike_payload[COS_V89_NUM_NEURONS];

    cos_v89_syn_t syn [COS_V89_MAX_SYN];
    uint32_t      syn_count;

    cos_v89_q16_t V_theta;
    cos_v89_q16_t V_max;
    int32_t       w_max;
    int32_t       A_plus;
    int32_t       A_minus;
    uint32_t      tau_mem_shift;        /* decay = V - (V>>tau_mem_shift) */
    uint32_t      step_t;
    uint64_t      spike_count;
    uint32_t      invariant_violations;
    uint32_t      sentinel;
} cos_v89_net_t;

/* Init.  V_theta=1.0 Q16.16, V_max=8.0, w_max=1.0, A_plus=1/64, A_minus=1/64. */
void cos_v89_net_init(cos_v89_net_t *n);

/* Add a directed synapse.  Returns synapse id, or UINT32_MAX if full. */
uint32_t cos_v89_connect(cos_v89_net_t *n, uint16_t pre, uint16_t post,
                         int32_t weight);

/* Inject an external graded input to neuron j. */
void cos_v89_inject(cos_v89_net_t *n, uint16_t j, int32_t payload);

/* One discrete timestep.  Delivers queued inputs, applies LIF decay,
 * fires threshold-crossings, propagates through synapses, runs STDP.
 * Returns the number of spikes in this step. */
uint32_t cos_v89_step(cos_v89_net_t *n);

/* Accessors. */
uint64_t cos_v89_spike_count(const cos_v89_net_t *n);
int32_t  cos_v89_weight     (const cos_v89_net_t *n, uint32_t syn_id);

/* Test helper: pure LIF decay step (τ_mem). */
cos_v89_q16_t cos_v89_lif_decay(cos_v89_q16_t V, uint32_t tau_shift);

/* Gate + compose. */
uint32_t cos_v89_ok(const cos_v89_net_t *n);
uint32_t cos_v89_compose_decision(uint32_t v88_composed_ok,
                                  uint32_t v89_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V89_SPIKING_H */
