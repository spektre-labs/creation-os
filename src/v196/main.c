/*
 * v196 σ-Habit — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "habit.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v196_self_test();
        if (rc == 0) puts("v196 self-test: OK");
        else         fprintf(stderr, "v196 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v196_state_t s;
    cos_v196_init(&s, 0x196A812ULL);
    cos_v196_build(&s);
    cos_v196_run(&s);
    char out[65536];
    size_t n = cos_v196_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
