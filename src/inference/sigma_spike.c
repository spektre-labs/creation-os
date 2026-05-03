/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * v154 σ-spike — LIF + Q16 gate validation + event-driven Ω chain (14 phases).
 * Mirrors ``src/sigma/sigma_spike.c`` gate arithmetic; does not modify ``sigma_gate.h``.
 */
#include "sigma_spike.h"

#include <string.h>

#define COS_K_CRIT ((int32_t)((int32_t)(0.127 * (double)COS_INFER_SPIKE_Q16)))

static void gate_reset(cos_sigma_gate_state_t *st)
{
    st->sigma   = 0;
    st->d_sigma = 0;
    st->k_eff   = COS_INFER_SPIKE_Q16;
}

static void gate_update(cos_sigma_gate_state_t *st, int32_t new_sigma_q16, int32_t k_raw_q16)
{
    int32_t delta = new_sigma_q16 - st->sigma;
    st->d_sigma = delta;
    st->sigma   = new_sigma_q16;
    {
        int64_t ke = ((int64_t)(COS_INFER_SPIKE_Q16 - st->sigma) * (int64_t)k_raw_q16) >> 16;
        st->k_eff  = (int32_t)ke;
    }
}

static int gate_should_suppress(const cos_sigma_gate_state_t *st)
{
    /* Aligns with ``sigma_spike.c``: sub-critical k_eff ⇒ ABSTAIN ⇒ do not propagate. */
    return st->k_eff < COS_K_CRIT;
}

static int32_t membrane_to_sigma_q16(int32_t membrane, int32_t threshold)
{
    int64_t num;
    int32_t thr = threshold > 0 ? threshold : 1;
    int32_t mcap = membrane;

    if (mcap > thr * 3)
        mcap = thr * 3;
    if (mcap <= 0)
        return 0;
    num = ((int64_t)mcap * (int64_t)COS_INFER_SPIKE_Q16) / ((int64_t)thr * 4LL);
    if (num > (int64_t)COS_INFER_SPIKE_Q16)
        return COS_INFER_SPIKE_Q16;
    if (num < 0)
        return 0;
    return (int32_t)num;
}

void cos_infer_sigma_lif_init(cos_infer_sigma_lif_neuron_t *n, int32_t threshold, int32_t leak,
                              int32_t refractory)
{
    if (!n)
        return;
    memset(n, 0, sizeof(*n));
    gate_reset(&n->gate);
    n->threshold   = threshold > 0 ? threshold : 100;
    n->leak        = leak >= 0 ? leak : 5;
    n->refractory  = refractory >= 0 ? refractory : 3;
    n->refrac_counter = 0;
}

void cos_infer_sigma_lif_idle(cos_infer_sigma_lif_neuron_t *n)
{
    if (!n)
        return;
    if (n->refrac_counter > 0) {
        n->refrac_counter--;
        n->spikes_suppressed++;
        return;
    }
    n->spikes_suppressed++;
}

int cos_infer_sigma_lif_step(cos_infer_sigma_lif_neuron_t *n, int32_t input, int32_t k_raw_q16)
{
    int32_t sigma_q16;

    if (!n)
        return 0;

    if (n->refrac_counter > 0) {
        n->refrac_counter--;
        n->spikes_suppressed++;
        return 0;
    }

    {
        int64_t m = (int64_t)n->membrane + (int64_t)input;
        if (m > (int64_t)2147483647LL)
            n->membrane = 2147483647;
        else if (m < (int64_t)-2147483647LL)
            n->membrane = -2147483647;
        else
            n->membrane = (int32_t)m;
    }

    n->membrane -= n->leak;
    if (n->membrane < 0)
        n->membrane = 0;

    if (n->membrane <= n->threshold) {
        n->spikes_suppressed++;
        return 0;
    }

    sigma_q16 = membrane_to_sigma_q16(n->membrane, n->threshold);
    gate_update(&n->gate, sigma_q16, k_raw_q16);

    if (gate_should_suppress(&n->gate)) {
        {
            int32_t half = n->threshold / 2;
            n->membrane = half > 0 ? half : 0;
        }
        n->spikes_suppressed++;
        return 0;
    }

    n->membrane      = 0;
    n->refrac_counter = n->refractory;
    n->spikes_fired++;
    return 1;
}

void cos_infer_sigma_spike_omega_init(cos_infer_sigma_spike_omega_t *o, int32_t threshold,
                                      int32_t leak, int32_t refractory)
{
    int i;
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    for (i = 0; i < (int)COS_INFER_SPIKE_N_PHASES; i++)
        cos_infer_sigma_lif_init(&o->neurons[i], threshold, leak, refractory);
}

void cos_infer_sigma_spike_omega_forward(cos_infer_sigma_spike_omega_t *o, int32_t input0,
                                         int32_t k_raw_q16)
{
    int     i;
    int32_t nxt = input0;

    if (!o)
        return;

    o->active_phases = 0;
    o->chain_passes  = 0;

    for (i = 0; i < (int)COS_INFER_SPIKE_N_PHASES; i++) {
        int sp = cos_infer_sigma_lif_step(&o->neurons[i], nxt, k_raw_q16);
        if (!sp) {
            i++;
            while (i < (int)COS_INFER_SPIKE_N_PHASES) {
                cos_infer_sigma_lif_idle(&o->neurons[i]);
                i++;
            }
            return;
        }
        o->active_phases++;
        o->chain_passes++;
        nxt = (int32_t)(((int64_t)nxt * 85LL) / 100LL);
        if (nxt < o->neurons[i].threshold / 4 + 1)
            nxt = o->neurons[i].threshold / 4 + 1;
    }
}

int32_t cos_infer_sigma_spike_omega_sparsity_q16(const cos_infer_sigma_spike_omega_t *o)
{
    uint64_t sup = 0;
    uint64_t tot = 0;
    int      i;

    if (!o)
        return 0;
    for (i = 0; i < (int)COS_INFER_SPIKE_N_PHASES; i++) {
        sup += (uint64_t)o->neurons[i].spikes_suppressed;
        tot += (uint64_t)o->neurons[i].spikes_fired + (uint64_t)o->neurons[i].spikes_suppressed;
    }
    if (tot == 0ULL)
        return 0;
    return (int32_t)((sup << 16) / tot);
}

float cos_infer_sigma_spike_omega_sparsity_f32(const cos_infer_sigma_spike_omega_t *o)
{
    int32_t q = cos_infer_sigma_spike_omega_sparsity_q16(o);
    return (float)q / (float)COS_INFER_SPIKE_Q16;
}
