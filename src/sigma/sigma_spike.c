/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/* ACSL companion (clause ledger): hw/formal/v133/sigma_stack_contracts.acsl */
/*
 * σ-spike implementation — event-thresholded calls into the same Q16 gate update as
 * ``omega_phase_gates.c`` / ``liquid_neuron.c`` (does not modify ``sigma_gate.h``).
 */
#include "sigma_spike.h"

#include <string.h>

#define COS_Q16 ((int32_t)65536)
#define COS_K_CRIT ((int32_t)((int32_t)(0.127 * (double)COS_Q16)))

typedef enum cos_spike_verdict {
    COS_SPIKE_V_ACCEPT  = 0,
    COS_SPIKE_V_RETHINK = 1,
    COS_SPIKE_V_ABSTAIN = 2,
} cos_spike_verdict_t;

static void gate_reset(cos_sigma_gate_state_t *st)
{
    st->sigma   = 0;
    st->d_sigma = 0;
    st->k_eff   = COS_Q16;
}

static void gate_update(cos_sigma_gate_state_t *st, int32_t new_sigma_q16, int32_t k_raw_q16)
{
    int32_t delta = new_sigma_q16 - st->sigma;
    st->d_sigma = delta;
    st->sigma   = new_sigma_q16;
    {
        int64_t ke = ((int64_t)(COS_Q16 - st->sigma) * (int64_t)k_raw_q16) >> 16;
        st->k_eff  = (int32_t)ke;
    }
}

static cos_spike_verdict_t gate_verdict(const cos_sigma_gate_state_t *st)
{
    if (st->k_eff < COS_K_CRIT)
        return COS_SPIKE_V_ABSTAIN;
    if (st->d_sigma > 0)
        return COS_SPIKE_V_RETHINK;
    return COS_SPIKE_V_ACCEPT;
}

void cos_sigma_spike_lane_init(cos_sigma_spike_lane_t *s, int32_t threshold_q16)
{
    if (!s)
        return;
    memset(s, 0, sizeof(*s));
    gate_reset(&s->gate);
    s->spike_threshold = threshold_q16 > 0 ? threshold_q16 : 1;
    s->last_verdict    = (int)COS_SPIKE_V_ACCEPT;
}

int cos_sigma_spike_lane_step(cos_sigma_spike_lane_t *s, int32_t input_q16, int32_t k_raw_q16)
{
    int32_t  d;
    int32_t  verdict_i;
    uint32_t total;

    if (!s)
        return (int)COS_SPIKE_V_ABSTAIN;

    d = input_q16 - s->last_input;
    if (d < 0)
        d = -d;
    total = s->spikes_fired + s->spikes_suppressed;
    if (total == 0U) {
        s->last_input = input_q16;
        s->spikes_fired++;
        gate_update(&s->gate, input_q16, k_raw_q16);
        verdict_i              = (int)gate_verdict(&s->gate);
        s->last_verdict      = verdict_i;
        return verdict_i;
    }

    if (d < s->spike_threshold) {
        s->spikes_suppressed++;
        return s->last_verdict;
    }

    s->last_input = input_q16;
    s->spikes_fired++;
    gate_update(&s->gate, input_q16, k_raw_q16);
    verdict_i         = (int)gate_verdict(&s->gate);
    s->last_verdict = verdict_i;
    return verdict_i;
}

int32_t cos_sigma_spike_lane_sparsity_q16(const cos_sigma_spike_lane_t *s)
{
    uint32_t total;
    if (!s)
        return 0;
    total = s->spikes_fired + s->spikes_suppressed;
    if (total == 0U)
        return 0;
    return (int32_t)(((int64_t)s->spikes_suppressed << 16) / (int64_t)total);
}

void cos_sigma_spike_omega_init(cos_sigma_spike_omega_bundle_t *b, int32_t threshold_q16)
{
    int i;
    if (!b)
        return;
    memset(b, 0, sizeof(*b));
    for (i = 0; i < (int)COS_OMEGA_N_PHASES; i++)
        cos_sigma_spike_lane_init(&b->lanes[i], threshold_q16);
}

void cos_sigma_spike_omega_step(cos_sigma_spike_omega_bundle_t *b, const int32_t *phase_sigma_q16,
                                int32_t k_raw_q16)
{
    int i;
    if (!b || !phase_sigma_q16)
        return;
    for (i = 0; i < (int)COS_OMEGA_N_PHASES; i++)
        (void)cos_sigma_spike_lane_step(&b->lanes[i], phase_sigma_q16[i], k_raw_q16);
}

int32_t cos_sigma_spike_omega_sparsity_q16(const cos_sigma_spike_omega_bundle_t *b)
{
    uint64_t sup = 0, tot = 0;
    int      i;
    if (!b)
        return 0;
    for (i = 0; i < (int)COS_OMEGA_N_PHASES; i++) {
        sup += (uint64_t)b->lanes[i].spikes_suppressed;
        tot += (uint64_t)b->lanes[i].spikes_fired + (uint64_t)b->lanes[i].spikes_suppressed;
    }
    if (tot == 0ULL)
        return 0;
    return (int32_t)((sup << 16) / tot);
}
