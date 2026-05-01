/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v172 σ-Narrative — CLI.
 *
 *   creation_os_v172_narrative --self-test
 *   creation_os_v172_narrative                # JSON state
 *   creation_os_v172_narrative --thread       # plaintext thread
 *   creation_os_v172_narrative --resume       # opener for a new session
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "narrative.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v172_self_test();
        if (rc == 0) puts("v172 self-test: ok");
        else         fprintf(stderr, "v172 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v172_state_t s;
    cos_v172_init(&s, 0x172FFFFFULL);

    if (argc >= 2 && strcmp(argv[1], "--thread") == 0) {
        char buf[4096];
        size_t n = cos_v172_narrative_thread(&s, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v172: thread overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "--resume") == 0) {
        char buf[COS_V172_MAX_RESUME];
        size_t n = cos_v172_resume(&s, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v172: resume overflow\n"); return 3; }
        fwrite(buf, 1, n, stdout); putchar('\n');
        return 0;
    }

    char buf[16384];
    size_t n = cos_v172_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v172: json overflow\n"); return 4; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
