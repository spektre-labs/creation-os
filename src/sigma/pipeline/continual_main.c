/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Continual self-test binary + canonical freeze demo. */

#include "continual.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_continual_self_test();

    const int N = 4;
    float imp[4];
    cos_sigma_continual_t c;
    cos_sigma_continual_init(&c, imp, N, 0.005f, 0.9f);

    float grad[4] = {1.0f, 0.0f, 0.5f, 0.0f};
    int zeros = cos_sigma_continual_step(&c, grad);

    printf("{\"kernel\":\"sigma_continual\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"n\":%d,"
             "\"freeze_threshold\":%.4f,"
             "\"decay\":%.4f,"
             "\"grad_masked\":%d,"
             "\"frozen_count\":%d,"
             "\"n_steps\":%d,"
             "\"importance\":[%.4f,%.4f,%.4f,%.4f]},"
           "\"pass\":%s}\n",
           rc, N,
           (double)c.freeze_threshold, (double)c.decay,
           zeros, c.frozen_count, c.n_steps,
           (double)imp[0], (double)imp[1],
           (double)imp[2], (double)imp[3],
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Continual demo:\n");
        for (int i = 0; i < N; ++i)
            fprintf(stderr, "  param[%d]  importance = %.4f  %s\n",
                i, (double)imp[i],
                (imp[i] > c.freeze_threshold) ? "FROZEN" : "free");
    }
    return rc;
}
