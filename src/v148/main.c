/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v148 σ-Sovereign — CLI.
 */
#include "sovereign.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --run [--cycles N] [--mode supervised|autonomous]\n"
        "               [--seed N] [--emergency-stop-after N]\n",
        a0, a0);
    return 2;
}

static int run(int cycles, const char *mode_str,
               uint64_t seed, int stop_after) {
    cos_v148_config_t cfg;
    cos_v148_default_config(&cfg);
    if (mode_str && !strcmp(mode_str, "autonomous"))
        cfg.mode = COS_V148_AUTONOMOUS;
    else
        cfg.mode = COS_V148_SUPERVISED;
    cfg.seed = seed;
    cos_v148_state_t s;
    cos_v148_init(&s, &cfg);

    printf("{\"v148_run\":true,\"seed\":%llu,\"mode\":%d,\"cycles\":[",
           (unsigned long long)seed, (int)cfg.mode);
    for (int k = 0; k < cycles; ++k) {
        if (stop_after > 0 && k == stop_after) {
            cos_v148_emergency_stop(&s);
        }
        cos_v148_cycle_report_t r;
        cos_v148_cycle(&s, &r);
        char buf[1400];
        cos_v148_cycle_to_json(&r, buf, sizeof buf);
        printf("%s%s", k ? "," : "", buf);
    }
    char stbuf[2048];
    cos_v148_state_to_json(&s, stbuf, sizeof stbuf);
    printf("],\"final_state\":%s}\n", stbuf);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v148_self_test();
    if (!strcmp(argv[1], "--run")) {
        int        cycles    = 6;
        const char *mode_str = "supervised";
        uint64_t   seed      = 0x501EC08ULL;
        int        stop      = -1;
        for (int i = 2; i < argc; ++i) {
            if (!strcmp(argv[i], "--cycles") && i + 1 < argc)
                cycles = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--mode") && i + 1 < argc)
                mode_str = argv[++i];
            else if (!strcmp(argv[i], "--seed") && i + 1 < argc)
                seed = strtoull(argv[++i], NULL, 0);
            else if (!strcmp(argv[i], "--emergency-stop-after") &&
                     i + 1 < argc)
                stop = atoi(argv[++i]);
        }
        if (cycles < 1)  cycles = 1;
        if (cycles > 64) cycles = 64;
        return run(cycles, mode_str, seed, stop);
    }
    return usage(argv[0]);
}
