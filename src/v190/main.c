/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v190 σ-Latent-Reason — CLI entry.
 *
 *   ./creation_os_v190_latent --self-test
 *   ./creation_os_v190_latent            # dump JSON report
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "latent.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v190_self_test();
        if (rc == 0) puts("v190 self-test: OK");
        else         fprintf(stderr, "v190 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v190_state_t s;
    cos_v190_init(&s, 0x190A7E47ULL);
    cos_v190_build(&s);
    cos_v190_run(&s);

    char out[32768];
    size_t n = cos_v190_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
