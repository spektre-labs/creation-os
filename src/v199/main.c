/*
 * v199 σ-Law — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "law.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v199_self_test();
        if (rc == 0) puts("v199 self-test: OK");
        else         fprintf(stderr, "v199 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v199_state_t s;
    cos_v199_init(&s, 0x1990A41ULL);
    cos_v199_build(&s);
    cos_v199_run(&s);
    char out[65536];
    size_t n = cos_v199_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
