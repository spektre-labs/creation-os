/*
 * v215 σ-Stigmergy — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "stigmergy.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v215_self_test();
        if (rc == 0) puts("v215 self-test: OK");
        else         fprintf(stderr, "v215 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v215_state_t s;
    cos_v215_init(&s, 0x215F7EA1ULL);
    cos_v215_build(&s);
    cos_v215_run(&s);
    char out[65536];
    size_t n = cos_v215_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
