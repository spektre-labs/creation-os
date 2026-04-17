/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v89 — σ-Spiking implementation.
 * Deterministic integer event-driven LIF + graded spikes + STDP.
 */

#include "spiking.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 *  Init                                                              *
 * ------------------------------------------------------------------ */

void cos_v89_net_init(cos_v89_net_t *n)
{
    memset(n, 0, sizeof(*n));
    n->sentinel = COS_V89_SENTINEL;
    n->V_theta = (cos_v89_q16_t)(1 << 16);       /* 1.0 */
    n->V_max   = (cos_v89_q16_t)(8 << 16);       /* 8.0 */
    n->w_max   = (int32_t)(1 << 16);             /* 1.0 */
    n->A_plus  = (int32_t)(1 << 10);             /* ~1/64 in Q16.16 */
    n->A_minus = (int32_t)(1 << 10);
    n->tau_mem_shift = 4u;                       /* V ← V - V/16 */

    for (uint32_t i = 0; i < COS_V89_NUM_NEURONS; ++i) {
        n->last_spike_t[i] = -1;
        n->last_spike_payload[i] = 0;
    }
}

/* ------------------------------------------------------------------ *
 *  Synapse management                                                *
 * ------------------------------------------------------------------ */

uint32_t cos_v89_connect(cos_v89_net_t *n, uint16_t pre, uint16_t post,
                         int32_t weight)
{
    if (n->sentinel != COS_V89_SENTINEL) {
        ++n->invariant_violations;
        return UINT32_MAX;
    }
    if (n->syn_count >= COS_V89_MAX_SYN) {
        ++n->invariant_violations;
        return UINT32_MAX;
    }
    if (pre >= COS_V89_NUM_NEURONS || post >= COS_V89_NUM_NEURONS) {
        ++n->invariant_violations;
        return UINT32_MAX;
    }

    /* Clamp weight. */
    if (weight >  n->w_max) weight =  n->w_max;
    if (weight < -n->w_max) weight = -n->w_max;

    uint32_t id = n->syn_count++;
    n->syn[id].pre = pre;
    n->syn[id].post = post;
    n->syn[id].weight = weight;
    return id;
}

/* ------------------------------------------------------------------ *
 *  Inject                                                            *
 * ------------------------------------------------------------------ */

void cos_v89_inject(cos_v89_net_t *n, uint16_t j, int32_t payload)
{
    if (n->sentinel != COS_V89_SENTINEL) { ++n->invariant_violations; return; }
    if (j >= COS_V89_NUM_NEURONS)        { ++n->invariant_violations; return; }
    int64_t v = (int64_t)n->V[j] + (int64_t)payload;
    if (v >  (int64_t)n->V_max) v =  n->V_max;
    if (v < -(int64_t)n->V_max) v = -n->V_max;
    n->V[j] = (cos_v89_q16_t)v;
}

/* ------------------------------------------------------------------ *
 *  LIF decay                                                         *
 * ------------------------------------------------------------------ */

cos_v89_q16_t cos_v89_lif_decay(cos_v89_q16_t V, uint32_t tau_shift)
{
    /* Arithmetic decay: V ← V - (V >> tau_shift).  Sign-preserving,
     * wrap-safe. */
    int32_t dV = V >> tau_shift;
    return (cos_v89_q16_t)((uint32_t)V - (uint32_t)dV);
}

/* ------------------------------------------------------------------ *
 *  One timestep: decay → fire → propagate → STDP                     *
 * ------------------------------------------------------------------ */

uint32_t cos_v89_step(cos_v89_net_t *n)
{
    if (n->sentinel != COS_V89_SENTINEL) {
        ++n->invariant_violations;
        return 0u;
    }

    /* 1. Leak all membranes. */
    for (uint32_t i = 0; i < COS_V89_NUM_NEURONS; ++i) {
        n->V[i] = cos_v89_lif_decay(n->V[i], n->tau_mem_shift);
    }

    /* 2. Fire: whichever V >= V_theta.  Graded payload = V - V_theta. */
    uint32_t fired[COS_V89_NUM_NEURONS];
    int32_t  payload[COS_V89_NUM_NEURONS];
    uint32_t num_fired = 0;

    for (uint32_t i = 0; i < COS_V89_NUM_NEURONS; ++i) {
        uint32_t fire = (n->V[i] >= n->V_theta) ? 1u : 0u;
        if (fire) {
            fired[num_fired] = i;
            payload[num_fired] = (int32_t)((uint32_t)n->V[i] - (uint32_t)n->V_theta);
            ++num_fired;
            /* Reset membrane to 0 on spike. */
            n->V[i] = 0;
            n->last_spike_t[i] = (int32_t)n->step_t;
            n->last_spike_payload[i] = payload[num_fired - 1];
            ++n->spike_count;
        }
    }

    /* 3. Propagate graded spikes through outgoing synapses. */
    for (uint32_t f = 0; f < num_fired; ++f) {
        uint32_t pre = fired[f];
        int32_t  p   = payload[f];
        for (uint32_t s = 0; s < n->syn_count; ++s) {
            if (n->syn[s].pre != pre) continue;
            uint16_t post = n->syn[s].post;
            /* Deliver weighted graded spike: ΔV = w * payload. */
            int64_t dv = ((int64_t)n->syn[s].weight * (int64_t)p) >> 16;
            int64_t v  = (int64_t)n->V[post] + dv;
            if (v >  (int64_t)n->V_max) v =  n->V_max;
            if (v < -(int64_t)n->V_max) v = -n->V_max;
            n->V[post] = (cos_v89_q16_t)v;
        }
    }

    /* 4. STDP: for each synapse, if pre and post have spiked within
     * the STDP window, adjust weight. */
    for (uint32_t s = 0; s < n->syn_count; ++s) {
        int32_t t_pre  = n->last_spike_t[n->syn[s].pre];
        int32_t t_post = n->last_spike_t[n->syn[s].post];
        if (t_pre < 0 || t_post < 0) continue;
        int32_t dt = t_post - t_pre;
        if (dt == 0) continue;
        int32_t adt = (dt < 0) ? -dt : dt;
        if ((uint32_t)adt > COS_V89_STDP_WINDOW) continue;

        int32_t dw;
        if (dt > 0) dw =  n->A_plus;   /* causal: strengthen */
        else        dw = -n->A_minus;  /* acausal: weaken */

        /* Apply and clamp. */
        int64_t new_w = (int64_t)n->syn[s].weight + (int64_t)dw;
        if (new_w >  (int64_t)n->w_max) new_w =  n->w_max;
        if (new_w < -(int64_t)n->w_max) new_w = -n->w_max;
        n->syn[s].weight = (int32_t)new_w;
    }

    /* 5. Monitor overflow invariant. */
    for (uint32_t i = 0; i < COS_V89_NUM_NEURONS; ++i) {
        int32_t V = n->V[i];
        int32_t aV = (V < 0) ? -V : V;
        if (aV > n->V_max) { ++n->invariant_violations; break; }
    }

    ++n->step_t;
    return num_fired;
}

/* ------------------------------------------------------------------ */
uint64_t cos_v89_spike_count(const cos_v89_net_t *n) { return n->spike_count; }

int32_t cos_v89_weight(const cos_v89_net_t *n, uint32_t syn_id)
{
    if (syn_id >= n->syn_count) return 0;
    return n->syn[syn_id].weight;
}

uint32_t cos_v89_ok(const cos_v89_net_t *n)
{
    uint32_t sent   = (n->sentinel == COS_V89_SENTINEL)  ? 1u : 0u;
    uint32_t noviol = (n->invariant_violations == 0u)    ? 1u : 0u;

    /* Weight-range invariant. */
    uint32_t w_ok = 1u;
    for (uint32_t s = 0; s < n->syn_count; ++s) {
        int32_t w = n->syn[s].weight;
        if (w > n->w_max || w < -n->w_max) { w_ok = 0u; break; }
    }
    return sent & noviol & w_ok;
}

uint32_t cos_v89_compose_decision(uint32_t v88_composed_ok, uint32_t v89_ok)
{
    return v88_composed_ok & v89_ok;
}
