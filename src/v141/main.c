/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v141 σ-Curriculum — CLI.
 */
#include "curriculum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --demo [--cycles N] [--alpha 0.4]\n",
        a0, a0);
    return 2;
}

static int demo(int cycles, float alpha) {
    cos_v141_state_t st;
    cos_v141_init_default(&st);
    printf("{\"v141_demo\":true,\"cycles\":[");
    for (int k = 0; k < cycles; ++k) {
        cos_v141_cycle_report_t r;
        if (cos_v141_cycle(&st, alpha, &r) != 0) return 1;
        char buf[1024];
        cos_v141_cycle_to_json(&r, &st, buf, sizeof buf);
        printf("%s%s", k ? "," : "", buf);
    }
    printf("]}\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v141_self_test();
    if (!strcmp(argv[1], "--demo")) {
        int   cycles = 5;
        float alpha  = 0.40f;
        for (int i = 2; i < argc; ++i) {
            if (!strcmp(argv[i], "--cycles") && i + 1 < argc) {
                cycles = atoi(argv[++i]);
                if (cycles < 1 || cycles > 64) cycles = 5;
            } else if (!strcmp(argv[i], "--alpha") && i + 1 < argc) {
                alpha = (float)atof(argv[++i]);
                if (alpha <= 0.0f || alpha > 1.0f) alpha = 0.40f;
            }
        }
        return demo(cycles, alpha);
    }
    return usage(argv[0]);
}
