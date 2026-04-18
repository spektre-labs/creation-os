/*
 * v174 σ-Flywheel — CLI.
 *
 *   creation_os_v174_flywheel --self-test
 *   creation_os_v174_flywheel                 # one cycle, JSON state
 *   creation_os_v174_flywheel --dpo           # NDJSON DPO dataset
 *   creation_os_v174_flywheel --baseline F    # set regression baseline
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "flywheel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v174_self_test();
        if (rc == 0) puts("v174 self-test: ok");
        else         fprintf(stderr, "v174 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v174_state_t s;
    cos_v174_init(&s, 0x174F1A1BEEFULL);

    bool dpo = false;
    float baseline = 0.0f;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dpo") == 0) dpo = true;
        else if (strcmp(argv[i], "--baseline") == 0 && i + 1 < argc)
            baseline = (float)strtod(argv[++i], NULL);
    }

    cos_v174_run_cycle(&s, baseline);

    if (dpo) {
        char buf[32768];
        size_t n = cos_v174_dpo_ndjson(&s, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v174: dpo overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout);
        return 0;
    }

    char buf[4096];
    size_t n = cos_v174_to_json(&s, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v174: json overflow\n"); return 3; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
