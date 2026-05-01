/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "bitnet_native.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    if (cos_bitnet_native_self_test() != 0) {
        fprintf(stderr, "FAIL bitnet_native self_test\n");
        return 1;
    }
    return 0;
}
