/*
 * v214 σ-Swarm-Evolve — CLI entry.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "swarm_evolve.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v214_self_test();
        if (rc == 0) puts("v214 self-test: OK");
        else         fprintf(stderr, "v214 self-test: FAIL (%d)\n", rc);
        return rc;
    }
    cos_v214_state_t s;
    cos_v214_init(&s, 0x214E4011ULL);
    cos_v214_build(&s);
    cos_v214_run(&s);
    char out[65536];
    size_t n = cos_v214_to_json(&s, out, sizeof(out));
    if (n == 0) return 1;
    fwrite(out, 1, n, stdout); fputc('\n', stdout);
    return 0;
}
