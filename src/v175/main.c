/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v175 σ-Debate-Train — CLI.
 *
 *   creation_os_v175_debate_train --self-test
 *   creation_os_v175_debate_train                # full JSON
 *   creation_os_v175_debate_train --dpo          # NDJSON DPO dataset
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "debate_train.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v175_self_test();
        if (rc == 0) puts("v175 self-test: ok");
        else         fprintf(stderr, "v175 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v175_state_t s;
    cos_v175_init(&s, 0x175BEEF0175ULL);
    cos_v175_run_debates   (&s);
    cos_v175_run_tournament(&s);
    cos_v175_run_spin      (&s);

    if (argc >= 2 && strcmp(argv[1], "--dpo") == 0) {
        char buf[16384];
        size_t n = cos_v175_dpo_ndjson(&s, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v175: dpo overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        return 0;
    }

    char buf[32768];
    size_t n = cos_v175_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v175: json overflow\n"); return 3; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
