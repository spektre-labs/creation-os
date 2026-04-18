/*
 * v184 σ-VLA — CLI entry.
 *
 *   ./creation_os_v184_vla --self-test
 *   ./creation_os_v184_vla            # full VLA JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "vla.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v184_self_test();
        if (rc == 0) puts("v184 self-test: OK");
        else         fprintf(stderr, "v184 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    cos_v184_state_t s;
    cos_v184_init(&s, 0x184BEADULL);
    cos_v184_build(&s);
    cos_v184_run(&s);
    char out[16384];
    size_t n = cos_v184_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
