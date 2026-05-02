/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../../src/inference/sigma_spike.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    cos_infer_sigma_spike_omega_t om;
    int32_t                       kraw = (int32_t)(65536 * 0.9);

    cos_infer_sigma_spike_omega_init(&om, 100, 5, 3);
    cos_infer_sigma_spike_omega_forward(&om, 250, kraw);
    if (om.active_phases == 0U) {
        fputs("infer_sigma_spike: expected at least one phase on moderate drive\n", stderr);
        return 1;
    }

    cos_infer_sigma_spike_omega_init(&om, 100, 5, 3);
    cos_infer_sigma_spike_omega_forward(&om, 250, kraw);
    if (om.active_phases < 4U) {
        fputs("infer_sigma_spike: strong drive should chain several phases\n", stderr);
        return 2;
    }

    (void)cos_infer_sigma_spike_omega_sparsity_f32(&om);

    puts("check_sigma_spike_infer_main: OK");
    return 0;
}
