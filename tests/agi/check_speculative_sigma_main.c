/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../../src/sigma/speculative_sigma.h"

#include <stdio.h>

int main(void)
{
    int rc = cos_speculative_sigma_self_test();
    if (rc != 0)
        fprintf(stderr, "cos_speculative_sigma_self_test failed rc=%d\n", rc);
    return rc != 0 ? 1 : 0;
}
