/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "omega_phase_gates.h"

#include <string.h>

#define COS_Q16 ((int32_t)65536)
#define COS_K_CRIT ((int32_t)((int32_t)(0.127 * (double)COS_Q16)))

typedef enum cos_omega_verdict {
    COS_OMEGA_V_ACCEPT = 0,
    COS_OMEGA_V_RETHINK = 1,
    COS_OMEGA_V_ABSTAIN = 2,
} cos_omega_verdict_t;

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

static cos_omega_verdict_t gate_verdict(const cos_sigma_gate_state_t *st)
{
    if (st->k_eff < COS_K_CRIT)
        return COS_OMEGA_V_ABSTAIN;
    if (st->d_sigma > 0)
        return COS_OMEGA_V_RETHINK;
    return COS_OMEGA_V_ACCEPT;
}

void cos_omega_phase_bundle_init(cos_omega_phase_bundle_t *b)
{
    int i;
    if (!b)
        return;
    memset(b, 0, sizeof(*b));
    gate_reset(&b->master);
    for (i = 0; i < (int)COS_OMEGA_N_PHASES; i++)
        gate_reset(&b->phases[i]);
    b->total_sigma_q16 = 0;
    b->turn = 0u;
}

int cos_omega_phase_step(cos_omega_phase_bundle_t *b, cos_omega_phase_id_t phase,
                         int32_t phase_sigma_q16, int32_t k_raw_q16)
{
    cos_omega_verdict_t v;
    if (!b)
        return (int)COS_OMEGA_V_ABSTAIN;
    if ((int)phase < 0 || (int)phase >= (int)COS_OMEGA_N_PHASES)
        return (int)COS_OMEGA_V_ABSTAIN;

    gate_update(&b->phases[phase], phase_sigma_q16, k_raw_q16);
    v = gate_verdict(&b->phases[phase]);

    if (b->total_sigma_q16 == 0)
        b->total_sigma_q16 = phase_sigma_q16;
    else
        b->total_sigma_q16 =
            (int32_t)(((int64_t)b->total_sigma_q16 + (int64_t)phase_sigma_q16) >> 1);

    gate_update(&b->master, b->total_sigma_q16, k_raw_q16);
    (void)gate_verdict(&b->master);
    return (int)v;
}
