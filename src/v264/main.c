/*
 * v264 σ-Sovereign-Stack — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "sovereign_stack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v264_self_test();
        if (rc == 0) puts("v264 self-test: OK");
        else         fprintf(stderr, "v264 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v264_state_t s;
    cos_v264_init(&s, 0x264505FEULL);
    cos_v264_run(&s);
    char *out = malloc(65536);
    if (!out) return 1;
    size_t n = cos_v264_to_json(&s, out, 65536);
    if (n == 0) { free(out); return 1; }
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    free(out);
    return 0;
}
