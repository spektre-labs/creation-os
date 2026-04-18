/*
 * v182 σ-Privacy — CLI entry.
 *
 *   ./creation_os_v182_privacy --self-test
 *   ./creation_os_v182_privacy                    # summary JSON
 *   ./creation_os_v182_privacy --forget 2026-04-18
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "privacy.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v182_self_test();
        if (rc == 0) puts("v182 self-test: OK");
        else         fprintf(stderr, "v182 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    cos_v182_state_t s;
    cos_v182_init(&s, 0x182FA11EBULL);
    cos_v182_populate_fixture(&s, 120);
    cos_v182_run_dp(&s);

    if (argc > 2 && strcmp(argv[1], "--forget") == 0) {
        cos_v182_forget(&s, argv[2]);
    }

    char out[1024];
    size_t n = cos_v182_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
