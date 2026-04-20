/*
 * v259 — sigma_measurement_t CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "sigma_measurement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v259_self_test();
        if (rc == 0) puts("v259 self-test: OK");
        else         fprintf(stderr, "v259 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v259_state_t s;
    cos_v259_init(&s, 0x25925925ULL);
    cos_v259_run(&s);
    char *out = malloc(8192);
    if (!out) return 1;
    size_t n = cos_v259_to_json(&s, out, 8192);
    if (n == 0) { free(out); return 1; }
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    free(out);
    return 0;
}
