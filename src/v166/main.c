/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v166 σ-Stream — CLI entry point.
 *
 *   creation_os_v166_stream --self-test
 *   creation_os_v166_stream --run "prompt words" [TAU=0.5] [BURST_SEQ=-1] [SEED=0x166...]
 *
 * Prints newline-delimited JSON: start frame, per-token frames
 * (token / interrupted), closing summary frame.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v166_self_test();
        if (rc == 0) puts("v166 self-test: ok");
        else         fprintf(stderr, "v166 self-test failed at stage %d\n", rc);
        return rc;
    }

    const char *prompt = "hello world of streaming inference";
    float tau = 0.5f;
    int burst = -1;
    uint64_t seed = 0x1660A7A7B8B8C9C9ULL;

    if (argc >= 3 && strcmp(argv[1], "--run") == 0) {
        prompt = argv[2];
        if (argc >= 4) tau  = (float)atof(argv[3]);
        if (argc >= 5) burst = atoi(argv[4]);
        if (argc >= 6) seed = strtoull(argv[5], NULL, 0);
    }

    cos_v166_stream_t s;
    cos_v166_run(&s, prompt, seed, tau, burst);

    char buf[16384];
    size_t n = cos_v166_stream_to_ndjson(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v166: ndjson overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout);
    return s.was_interrupted ? 1 : 0;
}
