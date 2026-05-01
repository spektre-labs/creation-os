/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v171 σ-Collab — CLI.
 *
 *   creation_os_v171_collab --self-test
 *   creation_os_v171_collab                        # scripted scenario (pair)
 *   creation_os_v171_collab --mode pair|lead|follow
 *   creation_os_v171_collab --audit                # NDJSON audit trail
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "collab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cos_v171_mode_t parse_mode(const char *s) {
    if (!s) return COS_V171_MODE_PAIR;
    if (strcmp(s, "lead")   == 0) return COS_V171_MODE_LEAD;
    if (strcmp(s, "follow") == 0) return COS_V171_MODE_FOLLOW;
    return COS_V171_MODE_PAIR;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v171_self_test();
        if (rc == 0) puts("v171 self-test: ok");
        else         fprintf(stderr, "v171 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v171_mode_t mode = COS_V171_MODE_PAIR;
    bool audit = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
            mode = parse_mode(argv[++i]);
        else if (strcmp(argv[i], "--audit") == 0)
            audit = true;
    }

    cos_v171_state_t s;
    cos_v171_init(&s, mode, 0x171BEEF0171ULL);
    cos_v171_run_scenario(&s);

    if (audit) {
        char buf[16384];
        size_t n = cos_v171_audit_ndjson(&s, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v171: audit overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        return 0;
    }

    char buf[16384];
    size_t n = cos_v171_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v171: json overflow\n"); return 3; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
