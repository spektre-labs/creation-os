/*
 * v210 σ-Guardian — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "guardian.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v210_self_test();
        if (rc == 0) puts("v210 self-test: OK");
        else         fprintf(stderr, "v210 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v210_state_t s;
    cos_v210_init(&s, 0x210CA12DULL);
    cos_v210_build(&s);
    cos_v210_run(&s);
    char out[65536];
    size_t n = cos_v210_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
