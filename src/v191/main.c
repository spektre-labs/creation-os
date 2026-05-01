/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v191 σ-Constitutional — CLI entry.
 *
 *   ./creation_os_v191_constitution --self-test
 *   ./creation_os_v191_constitution                # JSON report
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "constitution.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v191_self_test();
        if (rc == 0) puts("v191 self-test: OK");
        else         fprintf(stderr, "v191 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v191_state_t s;
    cos_v191_init(&s, 0x191C057FULL);
    cos_v191_build(&s);
    cos_v191_run(&s);

    char out[65536];
    size_t n = cos_v191_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
