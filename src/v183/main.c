/*
 * v183 σ-Governance-Theory — CLI entry.
 *
 *   ./creation_os_v183_governance --self-test
 *   ./creation_os_v183_governance            # full property JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "governance.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v183_self_test();
        if (rc == 0) puts("v183 self-test: OK");
        else         fprintf(stderr, "v183 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    cos_v183_state_t s;
    cos_v183_init(&s, 0x183E5A1F5ULL);
    cos_v183_run(&s);
    char out[8192];
    size_t n = cos_v183_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
