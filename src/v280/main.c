/*
 * v280 σ-MoE — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "moe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v280_self_test();
        if (rc == 0) puts("v280 self-test: OK");
        else         fprintf(stderr, "v280 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v280_state_t s;
    cos_v280_init(&s, 0x280666ULL);
    cos_v280_run(&s);
    char *out = malloc(65536);
    if (!out) return 1;
    size_t n = cos_v280_to_json(&s, out, 65536);
    if (n == 0) { free(out); return 1; }
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    free(out);
    return 0;
}
