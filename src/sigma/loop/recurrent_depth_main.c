/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "recurrent_depth.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_ultra_recurrent_depth_self_test();
    cos_ultra_recurrent_params_t p = {
        .t_max = 32, .tau_accept = 0.40f, .dim = 4,
        .h0_scale = 1.8f, .transform_gain = 0.87f, .pattern = 0,
    };
    int steps;
    cos_ultra_recurrent_stop_t why;
    float sigf, rho;
    if (cos_ultra_recurrent_depth_run(&p, &steps, &why, &sigf, &rho) != 0)
        return 1;
    const char *w = "max_iter";
    if (why == COS_ULTRA_REC_STOP_SIGMA_ACCEPT) w = "sigma_accept";
    else if (why == COS_ULTRA_REC_STOP_OVERTHINK) w = "overthink";
    else if (why == COS_ULTRA_REC_STOP_SPECTRAL) w = "spectral";
    printf("ULTRA-11 recurrent depth + σ-halting (demo)\n");
    printf("  steps=%d  stop=%s  sigma=%.4f  rho_hat=%.4f\n",
           steps, w, (double)sigf, (double)rho);
    return 0;
}
