/*
 * v227 σ-Entropy — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "entropy.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v227_self_test();
        if (rc == 0) puts("v227 self-test: OK");
        else         fprintf(stderr, "v227 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v227_state_t s;
    cos_v227_init(&s, 0x227E11EDULL);
    cos_v227_build(&s);
    cos_v227_run(&s);
    char out[65536];
    size_t n = cos_v227_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
