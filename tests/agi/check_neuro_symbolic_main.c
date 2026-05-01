/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "neural_symbolic.h"

#include "codex_smt.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    if (cos_codex_smt_self_test() != 0) {
        fprintf(stderr, "FAIL codex_smt self_test\n");
        return 1;
    }
    if (cos_neural_symbolic_self_test() != 0) {
        fprintf(stderr, "FAIL neural_symbolic self_test\n");
        return 2;
    }
    return 0;
}
