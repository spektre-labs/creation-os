/*
 * v239 σ-Compose-Runtime — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "runtime.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v239_self_test();
        if (rc == 0) puts("v239 self-test: OK");
        else         fprintf(stderr, "v239 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v239_state_t s;
    cos_v239_init(&s, 0x239BEEF0ULL);
    cos_v239_run(&s);
    char out[65536];
    size_t n = cos_v239_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
