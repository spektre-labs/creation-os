/*
 * v219 σ-Create — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "create.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v219_self_test();
        if (rc == 0) puts("v219 self-test: OK");
        else         fprintf(stderr, "v219 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v219_state_t s;
    cos_v219_init(&s, 0x219C4EA7ULL);
    cos_v219_build(&s);
    cos_v219_run(&s);
    char out[65536];
    size_t n = cos_v219_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
