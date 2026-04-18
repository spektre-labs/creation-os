/*
 * v231 σ-Immortal — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "immortal.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v231_self_test();
        if (rc == 0) puts("v231 self-test: OK");
        else         fprintf(stderr, "v231 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v231_state_t s;
    cos_v231_init(&s, 0x231BAC0FFEULL);
    cos_v231_run(&s);
    char out[16384];
    size_t n = cos_v231_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
