/*
 * v163 σ-Evolve-Architecture — CLI.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "evolve.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fputs(
      "usage: creation_os_v163_evolve --self-test\n"
      "       creation_os_v163_evolve --run   [--seed S]\n"
      "       creation_os_v163_evolve --auto  [--seed S] [--lat L] [--mem M]\n",
      stderr);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v163_self_test();
        printf("{\"kernel\":\"v163\",\"self_test\":\"%s\",\"rc\":%d}\n",
               rc == 0 ? "pass" : "fail", rc);
        return rc == 0 ? 0 : 1;
    }

    uint64_t seed = 163;
    int do_run = 0, do_auto = 0;
    float lat = 100.0f, mem = 400.0f;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--run"))  do_run  = 1;
        else if (!strcmp(argv[i], "--auto")) do_auto = 1;
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--lat")  && i + 1 < argc) lat = strtof(argv[++i], NULL);
        else if (!strcmp(argv[i], "--mem")  && i + 1 < argc) mem = strtof(argv[++i], NULL);
        else return usage();
    }

    cos_v163_search_t s;
    cos_v163_search_init(&s, seed);
    cos_v163_search_run(&s);

    if (do_auto) {
        cos_v163_auto_profile_t ap = cos_v163_auto_profile(&s, lat, mem);
        printf("{\"kernel\":\"v163\",\"auto_profile\":{"
               "\"latency_budget_ms\":%.1f,\"memory_budget_mb\":%.1f,"
               "\"accuracy\":%.4f,\"latency_ms\":%.2f,\"memory_mb\":%.1f,"
               "\"genes\":[",
               ap.latency_budget_ms, ap.memory_budget_mb,
               ap.choice.accuracy, ap.choice.latency_ms, ap.choice.memory_mb);
        bool first = true;
        for (int i = 0; i < COS_V163_N_GENES; ++i) {
            if (!ap.choice.genes[i]) continue;
            if (!first) printf(",");
            printf("\"%s\"", s.genes[i].id);
            first = false;
        }
        printf("]}}\n");
        return 0;
    }
    (void)do_run;
    char buf[65536];
    cos_v163_search_to_json(&s, buf, sizeof(buf));
    printf("%s\n", buf);
    return 0;
}
