/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "constitution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PA(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "FAIL %s\n", msg); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    PA(cos_constitution_self_test() == 0, "self_test");
    return 0;
}
