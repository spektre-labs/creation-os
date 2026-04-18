/*
 * v202 σ-Culture — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "culture.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v202_self_test();
        if (rc == 0) puts("v202 self-test: OK");
        else         fprintf(stderr, "v202 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v202_state_t s;
    cos_v202_init(&s, 0x202C1A7EULL);
    cos_v202_build(&s);
    cos_v202_run(&s);
    char out[131072];
    size_t n = cos_v202_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
