/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v179 σ-Interpret — CLI entry.
 *
 *   ./creation_os_v179_interpret --self-test
 *   ./creation_os_v179_interpret                 # features JSON
 *   ./creation_os_v179_interpret --explain N     # per-sample explain JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "interpret.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v179_self_test();
        if (rc == 0) puts("v179 self-test: OK");
        else         fprintf(stderr, "v179 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    cos_v179_state_t s;
    cos_v179_init(&s, 0x179E5AEFEA7ULL);
    cos_v179_generate_samples(&s);
    cos_v179_fit_sae(&s);

    if (argc > 2 && strcmp(argv[1], "--explain") == 0) {
        int idx = atoi(argv[2]);
        cos_v179_explain_t ex;
        if (cos_v179_explain(&s, idx, &ex) != 0) {
            fprintf(stderr, "v179: explain failed for sample %d\n", idx);
            return 1;
        }
        char out[4096];
        size_t n = cos_v179_explain_json(&ex, out, sizeof(out));
        if (n == 0) return 1;
        fwrite(out, 1, n, stdout); fputc('\n', stdout);
        return 0;
    }

    char out[16384];
    size_t n = cos_v179_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
