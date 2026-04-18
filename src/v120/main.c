/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v120 σ-Distill CLI.
 */
#include "distill.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --select IN.jsonl OUT.jsonl [TAU_BIG_LOW] [TAU_SMALL_HIGH]\n",
        argv0, argv0);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v120_self_test();
    if (!strcmp(argv[1], "--select") && argc >= 4) {
        cos_v120_config_t c;
        cos_v120_config_defaults(&c);
        if (argc >= 5) c.tau_big_low    = (float)atof(argv[4]);
        if (argc >= 6) c.tau_small_high = (float)atof(argv[5]);
        cos_v120_stats_t s;
        int rc = cos_v120_select(&c, argv[2], argv[3], &s);
        if (rc != 0) { fprintf(stderr, "select failed\n"); return 1; }
        char js[512];
        cos_v120_stats_to_json(&c, &s, js, sizeof js);
        puts(js);
        return 0;
    }
    return usage(argv[0]);
}
