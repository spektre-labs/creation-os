/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v189 σ-TTC — CLI entry.
 *
 *   ./creation_os_v189_ttc --self-test
 *   ./creation_os_v189_ttc                     # balanced (default)
 *   ./creation_os_v189_ttc --mode fast|balanced|deep
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ttc.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v189_self_test();
        if (rc == 0) puts("v189 self-test: OK");
        else         fprintf(stderr, "v189 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    int mode = COS_V189_MODE_BALANCED;
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--mode") == 0) {
            if      (strcmp(argv[i+1], "fast")     == 0) mode = COS_V189_MODE_FAST;
            else if (strcmp(argv[i+1], "balanced") == 0) mode = COS_V189_MODE_BALANCED;
            else if (strcmp(argv[i+1], "deep")     == 0) mode = COS_V189_MODE_DEEP;
        }
    }

    cos_v189_state_t s;
    cos_v189_init(&s, 0x189C7CAFULL, mode);
    cos_v189_build(&s);
    cos_v189_run(&s);

    char out[32768];
    size_t n = cos_v189_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
