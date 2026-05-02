/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../../src/sigma/sigma_spike.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    cos_sigma_spike_lane_t         lane;
    cos_sigma_spike_omega_bundle_t ob;
    int32_t                        ph[COS_OMEGA_N_PHASES];
    int                            i;
    int32_t                        kraw = (int32_t)(65536 * 0.9);

    cos_sigma_spike_lane_init(&lane, 500);
    (void)cos_sigma_spike_lane_step(&lane, 8000, kraw);
    (void)cos_sigma_spike_lane_step(&lane, 8000, kraw);
    if (lane.spikes_suppressed < 1U) {
        fputs("check_sigma_spike: expected suppression on stable input\n", stderr);
        return 1;
    }

    memset(ph, 0, sizeof ph);
    for (i = 0; i < (int)COS_OMEGA_N_PHASES; i++)
        ph[i] = (int32_t)(0.15 * 65536.0);

    cos_sigma_spike_omega_init(&ob, COS_SIGMA_SPIKE_Q16_SCALE / 512);
    cos_sigma_spike_omega_step(&ob, ph, kraw);
    cos_sigma_spike_omega_step(&ob, ph, kraw);
    if (cos_sigma_spike_omega_sparsity_q16(&ob) <= 0) {
        fputs("check_sigma_spike: expected omega sparsity after repeated turn\n", stderr);
        return 2;
    }

    puts("check_sigma_spike_main: OK");
    return 0;
}
