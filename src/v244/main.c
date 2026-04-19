/*
 * v244 σ-Package — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "package.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v244_self_test();
        if (rc == 0) puts("v244 self-test: OK");
        else         fprintf(stderr, "v244 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v244_state_full_t s;
    cos_v244_init(&s, 0x244A7EADULL);
    cos_v244_run(&s);
    char *out = malloc(131072);
    if (!out) return 1;
    size_t n = cos_v244_to_json(&s, out, 131072);
    if (n == 0) { free(out); return 1; }
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    free(out);
    return 0;
}
