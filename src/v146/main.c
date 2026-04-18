/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v146 σ-Genesis — CLI.
 */
#include "genesis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --demo [--version N] [--name NAME] [--gap STR] [--seed N] [--write DIR]\n"
        "       %s --pause-demo\n",
        a0, a0, a0);
    return 2;
}

static int demo(int version, const char *name, const char *gap,
                uint64_t seed, const char *write_dir) {
    cos_v146_state_t g;
    cos_v146_init(&g);

    char sk[COS_V146_SKELETON_CAP];
    int idx = cos_v146_generate(&g, version, name, gap, seed,
                                COS_V146_TAU_MERGE, sk, sizeof sk);
    if (idx < 0) {
        fprintf(stderr, "v146 demo: generation failed\n");
        return 1;
    }

    int wrote = 0;
    if (write_dir && *write_dir) {
        wrote = (cos_v146_write_candidate(&g, idx, write_dir) == 0)
                ? 1 : 0;
    }

    char js[2048];
    cos_v146_state_to_json(&g, js, sizeof js);
    printf("{\"v146_demo\":true,\"index\":%d,\"wrote_files\":%s,"
           "\"state\":%s}\n",
           idx, wrote ? "true" : "false", js);
    return 0;
}

static int pause_demo(void) {
    cos_v146_state_t g;
    cos_v146_init(&g);
    for (int k = 0; k < 3; ++k) {
        char nm[32];
        snprintf(nm, sizeof nm, "demo_%d", k);
        int idx = cos_v146_generate(&g, 160 + k, nm,
                                    "noisy gap", 0xDEEDF00Dull + k,
                                    0.999f, NULL, 0);
        cos_v146_reject(&g, idx);
    }
    char js[2048];
    cos_v146_state_to_json(&g, js, sizeof js);
    printf("{\"v146_pause_demo\":true,\"state\":%s}\n", js);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test"))  return cos_v146_self_test();
    if (!strcmp(argv[1], "--pause-demo")) return pause_demo();
    if (!strcmp(argv[1], "--demo")) {
        int         version = 149;
        const char *name    = "TemporalGrounding";
        const char *gap     =
            "temporal reasoning is weak on multi-step narratives";
        uint64_t    seed    = 0xDEEDF00Dull;
        const char *write_dir = NULL;
        for (int i = 2; i < argc; ++i) {
            if (!strcmp(argv[i], "--version") && i + 1 < argc)
                version = atoi(argv[++i]);
            else if (!strcmp(argv[i], "--name")  && i + 1 < argc)
                name = argv[++i];
            else if (!strcmp(argv[i], "--gap")   && i + 1 < argc)
                gap = argv[++i];
            else if (!strcmp(argv[i], "--seed")  && i + 1 < argc)
                seed = strtoull(argv[++i], NULL, 0);
            else if (!strcmp(argv[i], "--write") && i + 1 < argc)
                write_dir = argv[++i];
        }
        return demo(version, name, gap, seed, write_dir);
    }
    return usage(argv[0]);
}
