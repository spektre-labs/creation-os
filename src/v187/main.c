/*
 * v187 σ-Calibration — CLI entry.
 *
 *   ./creation_os_v187_calib --self-test
 *   ./creation_os_v187_calib            # full ECE + T JSON
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "calib.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v187_self_test();
        if (rc == 0) puts("v187 self-test: OK");
        else         fprintf(stderr, "v187 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v187_state_t s;
    cos_v187_init(&s, 0x187CA17BULL);
    cos_v187_build(&s);
    cos_v187_fit_global(&s);
    cos_v187_fit_domains(&s);
    cos_v187_rebin(&s);

    char out[32768];
    size_t n = cos_v187_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
