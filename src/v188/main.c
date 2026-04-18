/*
 * v188 σ-Alignment — CLI entry.
 *
 *   ./creation_os_v188_align --self-test
 *   ./creation_os_v188_align            # alignment report JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "align.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v188_self_test();
        if (rc == 0) puts("v188 self-test: OK");
        else         fprintf(stderr, "v188 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v188_state_t s;
    cos_v188_init(&s, 0x188A11611ULL);
    cos_v188_build(&s);
    cos_v188_measure(&s);

    char out[32768];
    size_t n = cos_v188_report_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
