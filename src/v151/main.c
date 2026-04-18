/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v151 σ-Code-Agent CLI wrapper.
 */
#include "codeagent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *p) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --tdd [--version N] [--name S] [--gap S]\n"
        "                 [--workroot DIR] [--tau T] [--seed N]\n"
        "       %s --pause-demo [--workroot DIR]\n",
        p, p, p);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 2; }

    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v151_self_test();
        printf("{\"kernel\":\"v151\",\"self_test\":%s,\"rc\":%d}\n",
               rc == 0 ? "\"pass\"" : "\"fail\"", rc);
        return rc;
    }

    int do_tdd = 0, do_pause = 0;
    int version = 300;
    const char *name = "probe";
    const char *gap  = "synthetic gap";
    const char *workroot = "/tmp/v151_codeagent";
    float tau = 0.35f;
    uint64_t seed = 0x1510ULL;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--tdd"))        do_tdd = 1;
        else if (!strcmp(a, "--pause-demo")) do_pause = 1;
        else if (!strcmp(a, "--version") && i + 1 < argc) version = atoi(argv[++i]);
        else if (!strcmp(a, "--name")    && i + 1 < argc) name    = argv[++i];
        else if (!strcmp(a, "--gap")     && i + 1 < argc) gap     = argv[++i];
        else if (!strcmp(a, "--workroot")&& i + 1 < argc) workroot= argv[++i];
        else if (!strcmp(a, "--tau")     && i + 1 < argc) tau     = (float)atof(argv[++i]);
        else if (!strcmp(a, "--seed")    && i + 1 < argc) seed    = strtoull(argv[++i], NULL, 0);
        else if (a[0] == '-' && strcmp(a, argv[0]) != 0) { usage(argv[0]); return 2; }
    }

    if (do_tdd) {
        cos_v151_agent_t agent;
        cos_v151_init(&agent, seed);
        cos_v151_tdd_report_t rep;
        int rc = cos_v151_run_tdd_cycle(&agent, version, name, gap,
                                        workroot, tau, &rep);
        if (rc != 0) {
            fprintf(stderr, "v151: run_tdd_cycle failed rc=%d\n", rc);
            return 3;
        }
        char buf[4096];
        cos_v151_report_to_json(&rep, buf, sizeof(buf));
        printf("%s\n", buf);
        return rep.status == COS_V151_STATUS_PENDING ? 0 : 0;
    }

    if (do_pause) {
        cos_v151_agent_t agent;
        cos_v151_init(&agent, seed);
        /* three quick rejects via v146 direct generate → reject */
        for (int i = 0; i < 3; ++i) {
            char skel[8];
            int idx = cos_v146_generate(&agent.genesis, 900 + i,
                                        "probe", "synthetic gap",
                                        (uint64_t)(seed + i),
                                        0.99f, skel, sizeof(skel));
            if (idx < 0) { fprintf(stderr, "genesis fail\n"); return 3; }
            cos_v151_tdd_report_t fake = {0};
            fake.candidate_idx = idx;
            fake.status = COS_V151_STATUS_PENDING;
            cos_v151_reject(&agent, &fake);
        }
        char buf[1024];
        cos_v151_agent_to_json(&agent, buf, sizeof(buf));
        printf("%s\n", buf);
        return 0;
    }

    usage(argv[0]);
    return 2;
}
