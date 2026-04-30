/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * Q16 gate mirror: matches ``python/cos/sigma_gate_core.py`` (Q16=65536, K_CRIT=0.127).
 */
#include "liquid_neuron.h"

#include <stdlib.h>

#define COS_Q16 ((int32_t)65536)
#define COS_K_CRIT ((int32_t)((int32_t)(0.127 * (double)COS_Q16)))

typedef enum cos_verdict {
    COS_VERDICT_ACCEPT = 0,
    COS_VERDICT_RETHINK = 1,
    COS_VERDICT_ABSTAIN = 2,
} cos_verdict_t;

static void gate_reset(cos_sigma_gate_state_t *s)
{
    s->sigma = 0;
    s->d_sigma = 0;
    s->k_eff = COS_Q16;
}

static void gate_update(cos_sigma_gate_state_t *st, int32_t new_sigma_q16, int32_t k_raw_q16)
{
    int32_t delta = new_sigma_q16 - st->sigma;
    st->d_sigma = delta;
    st->sigma = new_sigma_q16;
    {
        int64_t ke = ((int64_t)(COS_Q16 - st->sigma) * (int64_t)k_raw_q16) >> 16;
        st->k_eff = (int32_t)ke;
    }
}

static cos_verdict_t gate_verdict(const cos_sigma_gate_state_t *st)
{
    if (st->k_eff < COS_K_CRIT)
        return COS_VERDICT_ABSTAIN;
    if (st->d_sigma > 0)
        return COS_VERDICT_RETHINK;
    return COS_VERDICT_ACCEPT;
}

void liquid_sigma_neuron_init(liquid_sigma_neuron_t *n, int32_t tau_q16)
{
    if (n == NULL)
        return;
    n->state = 0;
    n->tau = tau_q16 > 0 ? tau_q16 : (COS_Q16 / 8);
    n->pad = 0;
    gate_reset(&n->gate);
}

int32_t liquid_sigma_neuron_update(liquid_sigma_neuron_t *n, int32_t input_q16)
{
    int64_t delta;
    int32_t err;
    int32_t sigma_q16;
    cos_verdict_t v;

    if (n == NULL)
        return 0;

    delta = ((int64_t)(input_q16 - n->state) * (int64_t)n->tau) >> 16;
    n->state = (int32_t)(n->state + (int32_t)delta);

    err = input_q16 - n->state;
    if (err < 0)
        err = -err;
    if (err >= COS_Q16)
        err = COS_Q16 - 1;
    sigma_q16 = err;

    gate_update(&n->gate, sigma_q16, COS_Q16);
    v = gate_verdict(&n->gate);
    if (v == COS_VERDICT_ABSTAIN)
        return 0;
    return n->state;
}
