/*
 * v201 σ-Diplomacy — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "diplomacy.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v201_self_test();
        if (rc == 0) puts("v201 self-test: OK");
        else         fprintf(stderr, "v201 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v201_state_t s;
    cos_v201_init(&s, 0x201D191DULL);
    cos_v201_build(&s);
    cos_v201_run(&s);
    char out[65536];
    size_t n = cos_v201_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
