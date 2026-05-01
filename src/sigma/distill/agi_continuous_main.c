/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* AGI-3 — `cos distill status` driver. */

#include "agi_continuous.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0)
        return cos_agi_distill_self_test() != 0 ? 1 : 0;
    if (argc >= 2 && (strcmp(argv[1], "status") == 0 ||
                      strcmp(argv[1], "--status") == 0)) {
        if (cos_agi_distill_emit_status(stdout) != 0) return 1;
        return 0;
    }
    /* Default: same as `distill status`. */
    if (cos_agi_distill_emit_status(stdout) != 0) return 1;
    return 0;
}
