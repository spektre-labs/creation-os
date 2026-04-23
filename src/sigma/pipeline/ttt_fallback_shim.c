/*
 * Fallback when linking pipeline + ttt_runtime without bitnet_server.o
 * (standalone pipeline self-test / integration harness).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "bitnet_server.h"

int cos_bitnet_server_copy_last_token_sigmas(float *dst, int cap,
                                             int *n_out) {
    (void)dst;
    (void)cap;
    if (n_out != NULL)
        *n_out = 0;
    return 0;
}
