/*
 * v241 σ-API-Surface — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "api.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v241_self_test();
        if (rc == 0) puts("v241 self-test: OK");
        else         fprintf(stderr, "v241 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v241_state_t s;
    cos_v241_init(&s, 0x241A91E0ULL);
    cos_v241_run(&s);
    char out[32768];
    size_t n = cos_v241_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
