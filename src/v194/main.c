/*
 * v194 σ-Horizon — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "horizon.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v194_self_test();
        if (rc == 0) puts("v194 self-test: OK");
        else         fprintf(stderr, "v194 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v194_state_t s;
    cos_v194_init(&s, 0x194ABCDEULL);
    cos_v194_build(&s);
    cos_v194_run(&s);
    char out[65536];
    size_t n = cos_v194_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
