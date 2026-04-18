/*
 * v228 σ-Unified — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "unified.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v228_self_test();
        if (rc == 0) puts("v228 self-test: OK");
        else         fprintf(stderr, "v228 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v228_state_t s;
    cos_v228_init(&s, 0x228F1E1DULL);
    cos_v228_run(&s);
    char out[262144];
    size_t n = cos_v228_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
