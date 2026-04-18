/*
 * v220 σ-Simulate — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "simulate.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v220_self_test();
        if (rc == 0) puts("v220 self-test: OK");
        else         fprintf(stderr, "v220 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v220_state_t s;
    cos_v220_init(&s, 0x220E5171ULL);
    cos_v220_build(&s);
    cos_v220_run(&s);
    char out[65536];
    size_t n = cos_v220_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
