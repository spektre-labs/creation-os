/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v181 σ-Audit — CLI entry.
 *
 *   ./creation_os_v181_audit --self-test
 *   ./creation_os_v181_audit                 # summary JSON
 *   ./creation_os_v181_audit --export        # JSONL dump
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v181_self_test();
        if (rc == 0) puts("v181 self-test: OK");
        else         fprintf(stderr, "v181 self-test: FAIL (%d)\n", rc);
        return rc;
    }

    cos_v181_state_t s;
    if (cos_v181_init(&s, COS_V181_MAX_ENTRIES, 0x181AFEDCULL) != 0)
        return 1;
    cos_v181_populate_fixture(&s, 1000);
    cos_v181_detect_anomaly(&s);

    if (argc > 1 && strcmp(argv[1], "--export") == 0) {
        size_t cap = 2 * 1024 * 1024;
        char  *jb  = (char *)malloc(cap);
        if (!jb) { cos_v181_free(&s); return 1; }
        size_t w = cos_v181_export_jsonl(&s, jb, cap);
        if (w == 0) { free(jb); cos_v181_free(&s); return 1; }
        fwrite(jb, 1, w, stdout);
        free(jb); cos_v181_free(&s);
        return 0;
    }

    char out[1024];
    size_t n = cos_v181_to_json(&s, out, sizeof(out));
    cos_v181_free(&s);
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
