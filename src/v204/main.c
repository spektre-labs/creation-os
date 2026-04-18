/*
 * v204 σ-Hypothesis — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "hypothesis.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v204_self_test();
        if (rc == 0) puts("v204 self-test: OK");
        else         fprintf(stderr, "v204 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v204_state_t s;
    cos_v204_init(&s, 0x2040471DULL);
    cos_v204_build(&s);
    cos_v204_run(&s);
    char out[65536];
    size_t n = cos_v204_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
