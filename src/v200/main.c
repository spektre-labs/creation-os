/*
 * v200 σ-Market — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "market.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v200_self_test();
        if (rc == 0) puts("v200 self-test: OK");
        else         fprintf(stderr, "v200 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v200_state_t s;
    cos_v200_init(&s, 0x200EA121ULL);
    cos_v200_build(&s);
    cos_v200_run(&s);
    char out[131072];
    size_t n = cos_v200_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
