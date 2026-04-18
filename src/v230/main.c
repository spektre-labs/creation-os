/*
 * v230 σ-Fork — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "fork.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v230_self_test();
        if (rc == 0) puts("v230 self-test: OK");
        else         fprintf(stderr, "v230 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v230_state_t s;
    cos_v230_init(&s, 0x230F034BULL);
    cos_v230_run(&s);
    char out[16384];
    size_t n = cos_v230_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
