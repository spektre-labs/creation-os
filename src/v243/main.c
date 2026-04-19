/*
 * v243 σ-Complete — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "complete.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v243_self_test();
        if (rc == 0) puts("v243 self-test: OK");
        else         fprintf(stderr, "v243 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v243_state_t s;
    cos_v243_init(&s, 0x243C0DE0ULL);
    cos_v243_run(&s);
    char out[32768];
    size_t n = cos_v243_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
