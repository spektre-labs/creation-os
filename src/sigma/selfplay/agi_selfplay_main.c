/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* cos selfplay — AGI-2 CLI (σ-guided curriculum). */

#include "agi_selfplay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr,
        "creation_os_agi_selfplay — σ-guided self-play (AGI-2)\n"
        "  --rounds N       (default: 32)\n"
        "  --domain NAME    (default: math)\n"
        "  --escalate-at F  σ threshold counting synthetic escalations "
        "(default: 0.72)\n"
        "  --self-test\n"
        "  --help\n");
}

int main(int argc, char **argv) {
    int   rounds = 32;
    const char *domain = "math";
    float esc_th = 0.72f;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--rounds") && i + 1 < argc)
            rounds = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--domain") && i + 1 < argc)
            domain = argv[++i];
        else if (!strcmp(argv[i], "--escalate-at") && i + 1 < argc)
            esc_th = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--self-test")) {
            int rc = cos_agi_selfplay_self_test();
            printf("agi-selfplay: self-test %s\n", rc == 0 ? "PASS" : "FAIL");
            return rc == 0 ? 0 : 1;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            usage();
            return 2;
        }
    }
    if (rounds < 1) rounds = 1;
    if (rounds > 100000) rounds = 100000;

    cos_agi_selfplay_report_t rep;
    if (cos_agi_selfplay_run(domain, rounds, esc_th, &rep) != 0) {
        fprintf(stderr, "agi-selfplay: run failed\n");
        return 1;
    }

    printf("{\n"
           "  \"schema\": \"cos.agi_selfplay.v1\",\n"
           "  \"domain\": \"%s\",\n"
           "  \"rounds\": %d,\n"
           "  \"sigma_mean\": %.6f,\n"
           "  \"sigma_target_end\": %.6f,\n"
           "  \"difficulty_end\": %.6f,\n"
           "  \"escalations\": %d,\n"
           "  \"escalation_threshold\": %.6f\n"
           "}\n",
           domain, rep.n_rounds, (double)rep.sigma_mean,
           (double)rep.sigma_target_end, (double)rep.difficulty_end,
           rep.n_escalations, (double)esc_th);
    return 0;
}
