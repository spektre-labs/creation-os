/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v145 σ-Skill — CLI.
 */
#include "skill.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --demo [--seed 0xC05]\n"
        "       %s --route TOPIC [--seed 0xC05]\n",
        a0, a0, a0);
    return 2;
}

static int demo(uint64_t seed) {
    cos_v145_library_t lib;
    cos_v145_seed_default(&lib, seed);

    /* Stack math+code; share at τ=0.35; try to evolve the math skill. */
    int mi = cos_v145_route(&lib, "math");
    int ci = cos_v145_route(&lib, "code");
    float sig_stacked = 0.0f;
    int pair[2] = { mi, ci };
    cos_v145_stack(&lib, pair, 2, &sig_stacked);

    int n_shared = cos_v145_share(&lib, COS_V145_TAU_SHARE);

    int evolved = 0;
    for (uint64_t s = 1; s <= 8 && !evolved; ++s) {
        if (cos_v145_evolve(&lib, mi, s) == 1) evolved = 1;
    }

    char buf[4096];
    cos_v145_library_to_json(&lib, buf, sizeof buf);
    printf("{\"v145_demo\":true,\"seed\":%llu,"
           "\"route_math\":%d,\"route_code\":%d,"
           "\"stack_math_code_sigma\":%.6f,"
           "\"shared_at_0.35\":%d,\"evolved_math\":%s,"
           "\"library\":%s}\n",
           (unsigned long long)seed, mi, ci,
           (double)sig_stacked, n_shared,
           evolved ? "true" : "false", buf);
    return 0;
}

static int route_cmd(const char *topic, uint64_t seed) {
    cos_v145_library_t lib;
    cos_v145_seed_default(&lib, seed);
    int idx = cos_v145_route(&lib, topic);
    if (idx < 0) {
        printf("{\"v145_route\":true,\"topic\":\"%s\",\"hit\":false}\n",
               topic);
        return 0;
    }
    char buf[1024];
    cos_v145_skill_to_json(&lib.skills[idx], buf, sizeof buf);
    printf("{\"v145_route\":true,\"topic\":\"%s\",\"hit\":true,"
           "\"index\":%d,\"skill\":%s}\n",
           topic, idx, buf);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v145_self_test();

    uint64_t seed = 0xC05ULL;
    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = strtoull(argv[++i], NULL, 0);
        }
    }

    if (!strcmp(argv[1], "--demo")) return demo(seed);
    if (!strcmp(argv[1], "--route") && argc >= 3) {
        return route_cmd(argv[2], seed);
    }
    return usage(argv[0]);
}
