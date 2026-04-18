/*
 * v192 σ-Emergent — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "emergent.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v192_self_test();
        if (rc == 0) puts("v192 self-test: OK");
        else         fprintf(stderr, "v192 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v192_state_t s;
    cos_v192_init(&s, 0x192E3E7ULL);
    cos_v192_build(&s);
    cos_v192_run(&s);

    char out[32768];
    size_t n = cos_v192_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
