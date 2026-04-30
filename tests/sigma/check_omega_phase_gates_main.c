/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../../src/sigma/omega_phase_gates.h"

#include <stdio.h>

int main(void)
{
    cos_omega_phase_bundle_t b;
    int                      i, v;

    cos_omega_phase_bundle_init(&b);
    for (i = 0; i < (int)COS_OMEGA_N_PHASES; i++) {
        int32_t sig = (int32_t)(65536 * 0.12); /* low σ */
        v = cos_omega_phase_step(&b, (cos_omega_phase_id_t)i, sig, 65536 - 1);
        if (v == (int)2)
            return 1;
    }
    puts("check_omega_phase_gates_main: OK");
    return 0;
}
