/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "text_similarity.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int f = cos_text_similarity_self_check();
    if (f != 0) {
        fprintf(stderr, "check-text-similarity: FAIL (%d)\n", f);
        return 1;
    }
    return 0;
}
