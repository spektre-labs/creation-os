/*
 * v233 σ-Legacy — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "legacy.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v233_self_test();
        if (rc == 0) puts("v233 self-test: OK");
        else         fprintf(stderr, "v233 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v233_state_t s;
    cos_v233_init(&s, 0x233DEAD5ULL);
    cos_v233_run(&s);
    char out[16384];
    size_t n = cos_v233_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
