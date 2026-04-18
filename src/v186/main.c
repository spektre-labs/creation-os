/*
 * v186 σ-Continual-Architecture — CLI entry.
 *
 *   ./creation_os_v186_grow --self-test
 *   ./creation_os_v186_grow            # run + JSON architecture history
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "grow.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v186_self_test();
        if (rc == 0) puts("v186 self-test: OK");
        else         fprintf(stderr, "v186 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    cos_v186_state_t s;
    cos_v186_init(&s, 0x186DA710ULL);
    cos_v186_run(&s);

    char out[32768];
    size_t n = cos_v186_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
