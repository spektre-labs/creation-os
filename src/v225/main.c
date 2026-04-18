/*
 * v225 σ-Fractal — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "fractal.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v225_self_test();
        if (rc == 0) puts("v225 self-test: OK");
        else         fprintf(stderr, "v225 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v225_state_t s;
    cos_v225_init(&s, 0x225F4AC7ULL);
    cos_v225_build(&s);
    cos_v225_run(&s);
    char out[65536];
    size_t n = cos_v225_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
