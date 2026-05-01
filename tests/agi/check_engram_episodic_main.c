/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "engram_episodic.h"
#include <stdio.h>

int main(void) {
    int rc = cos_engram_episodic_self_test();
    if (rc != 0)
        fprintf(stderr, "check-engram-episodic failed rc=%d\n", rc);
    return rc != 0 ? 1 : 0;
}
