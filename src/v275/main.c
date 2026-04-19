/*
 * v275 σ-TTT — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ttt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v275_self_test();
        if (rc == 0) puts("v275 self-test: OK");
        else         fprintf(stderr, "v275 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v275_state_t s;
    cos_v275_init(&s, 0x275777ULL);
    cos_v275_run(&s);
    char *out = malloc(65536);
    if (!out) return 1;
    size_t n = cos_v275_to_json(&s, out, 65536);
    if (n == 0) { free(out); return 1; }
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    free(out);
    return 0;
}
