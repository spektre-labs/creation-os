/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v170 σ-Transfer — CLI.
 *
 *   creation_os_v170_transfer --self-test
 *   creation_os_v170_transfer                   # state JSON
 *   creation_os_v170_transfer --transfer NAME   # run decide+apply
 *   creation_os_v170_transfer --zero-shot NAME  # ensemble σ for unseen
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "transfer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v170_self_test();
        if (rc == 0) puts("v170 self-test: ok");
        else         fprintf(stderr, "v170 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v170_state_t s;
    cos_v170_init(&s, 0x1700ACAB0170ULL);

    if (argc >= 3 && strcmp(argv[1], "--transfer") == 0) {
        char buf[4096];
        size_t n = cos_v170_report_json(&s, argv[2], buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v170: bad target\n"); return 2; }
        fwrite(buf, 1, n, stdout); putchar('\n');
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "--zero-shot") == 0) {
        int idx = cos_v170_find_domain(&s, argv[2]);
        if (idx < 0) { fprintf(stderr, "v170: bad domain\n"); return 3; }
        s.domains[idx].sigma = 1.0f;
        int k = 0;
        float zs = cos_v170_zero_shot(&s, idx, &k);
        printf("{\"kernel\":\"v170\",\"event\":\"zero_shot\","
               "\"target\":\"%s\",\"k\":%d,\"sigma_ensemble\":%.4f}\n",
               argv[2], k, (double)zs);
        return 0;
    }

    char buf[4096];
    size_t n = cos_v170_state_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v170: json overflow\n"); return 4; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
