/*
 * v154 σ-Showcase — CLI entry point.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "showcase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fputs(
      "usage: creation_os_v154_showcase [--self-test]\n"
      "       creation_os_v154_showcase --scenario {research|coder|collab}\n"
      "                                 [--seed N] [--tau 0.60]\n"
      "       creation_os_v154_showcase --demo-all [--seed N]\n",
      stderr);
    return 2;
}

static int demo_one(cos_v154_scenario_t s, uint64_t seed, float tau) {
    cos_v154_report_t r;
    if (!cos_v154_run(s, seed, tau, &r)) {
        fprintf(stderr, "v154: invalid scenario\n");
        return 1;
    }
    char buf[8192];
    cos_v154_report_to_json(&r, buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v154_self_test();
        printf("{\"kernel\":\"v154\",\"self_test\":\"%s\",\"rc\":%d}\n",
               rc == 0 ? "pass" : "fail", rc);
        return rc == 0 ? 0 : 1;
    }

    uint64_t seed = 154;
    float    tau  = COS_V154_TAU_ABSTAIN_DEF;
    const char *scen = NULL;
    int demo_all = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--scenario") && i + 1 < argc) {
            scen = argv[++i];
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--tau") && i + 1 < argc) {
            tau = (float)strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--demo-all")) {
            demo_all = 1;
        } else {
            return usage();
        }
    }

    if (demo_all) {
        int rc = 0;
        rc |= demo_one(COS_V154_SCENARIO_RESEARCH, seed, tau);
        rc |= demo_one(COS_V154_SCENARIO_CODER,    seed, tau);
        rc |= demo_one(COS_V154_SCENARIO_COLLAB,   seed, tau);
        return rc;
    }
    if (!scen) return usage();
    if (!strcmp(scen, "research")) return demo_one(COS_V154_SCENARIO_RESEARCH, seed, tau);
    if (!strcmp(scen, "coder"))    return demo_one(COS_V154_SCENARIO_CODER,    seed, tau);
    if (!strcmp(scen, "collab"))   return demo_one(COS_V154_SCENARIO_COLLAB,   seed, tau);
    return usage();
}
