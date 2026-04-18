/*
 * v180 σ-Steer — CLI entry.
 *
 *   ./creation_os_v180_steer --self-test
 *   ./creation_os_v180_steer                   # aggregated JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "steer.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v180_self_test();
        if (rc == 0) puts("v180 self-test: OK");
        else         fprintf(stderr, "v180 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    cos_v180_state_t s;
    cos_v180_init(&s, 0x180DA1ECF11EULL);
    cos_v180_build_vectors(&s);
    cos_v180_build_samples(&s);
    cos_v180_run_truthful(&s);
    cos_v180_run_no_firmware(&s);
    cos_v180_run_expert_ladder(&s);

    char out[4096];
    size_t n = cos_v180_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
