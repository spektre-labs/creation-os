/*
 * v232 σ-Lineage — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "lineage.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v232_self_test();
        if (rc == 0) puts("v232 self-test: OK");
        else         fprintf(stderr, "v232 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v232_state_t s;
    cos_v232_init(&s, 0x232AFCED5ULL);
    cos_v232_run(&s);
    char out[16384];
    size_t n = cos_v232_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
