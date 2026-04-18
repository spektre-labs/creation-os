/*
 * v173 σ-Teach — CLI.
 *
 *   creation_os_v173_teach --self-test
 *   creation_os_v173_teach                   # baked session JSON
 *   creation_os_v173_teach --trace           # NDJSON event trace
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "teach.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v173_self_test();
        if (rc == 0) puts("v173 self-test: ok");
        else         fprintf(stderr, "v173 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v173_state_t s;
    cos_v173_init(&s, "σ-theory", 0x173FACE0173ULL);
    cos_v173_fixture_sigma_theory(&s);

    float diag[COS_V173_N_SUBTOPICS] = { 0.80f, 0.55f, 0.25f, 0.10f };
    cos_v173_socratic(&s, diag);
    cos_v173_teach_cycle(&s);

    if (argc >= 2 && strcmp(argv[1], "--trace") == 0) {
        char buf[16384];
        size_t n = cos_v173_trace_ndjson(&s, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v173: trace overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        return 0;
    }

    char buf[8192];
    size_t n = cos_v173_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v173: json overflow\n"); return 3; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
