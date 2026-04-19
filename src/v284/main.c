/*
 * v284 σ-Multi-Agent — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "multi_agent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v284_self_test();
        if (rc == 0) puts("v284 self-test: OK");
        else         fprintf(stderr, "v284 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v284_state_t s;
    cos_v284_init(&s, 0x284222ULL);
    cos_v284_run(&s);
    char *out = malloc(65536);
    if (!out) return 1;
    size_t n = cos_v284_to_json(&s, out, 65536);
    if (n == 0) { free(out); return 1; }
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    free(out);
    return 0;
}
