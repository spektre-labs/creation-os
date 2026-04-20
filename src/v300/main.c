/*
 * v300 σ-Complete — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "complete.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v300_self_test();
        if (rc == 0) puts("v300 self-test: OK");
        else         fprintf(stderr, "v300 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v300_state_t s;
    cos_v300_init(&s, 0x300205ULL);
    cos_v300_run(&s);
    char *out = malloc(131072);
    if (!out) return 1;
    size_t n = cos_v300_to_json(&s, out, 131072);
    if (n == 0) { free(out); return 1; }
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    free(out);
    return 0;
}
