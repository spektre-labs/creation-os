/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v119 σ-Speculative CLI: self-test and synthetic simulator.
 */
#include "speculative.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --simulate N SIGMA          # synthetic stream, fixed σ\n"
        "  %s --gamma SIGMA               # adaptive γ for a given σ\n",
        argv0, argv0, argv0);
    return 2;
}

static int cmd_simulate(int n, float sigma) {
    if (n <= 0 || n > 1000000) { fprintf(stderr, "N out of range\n"); return 2; }
    cos_v119_config_t c;
    cos_v119_config_defaults(&c);
    cos_v119_draft_token_t  *d = calloc((size_t)n, sizeof *d);
    cos_v119_verify_token_t *v = calloc((size_t)n, sizeof *v);
    if (!d || !v) { free(d); free(v); return 1; }

    /* Every 7th token is a target-argmax mismatch so both accept and
     * reject paths exercise under confident σ; uncertain σ will be
     * dominated by σ-gates. */
    for (int i = 0; i < n; ++i) {
        d[i].token_id      = 1000 + i;
        d[i].p_draft       = 0.85f;
        d[i].sigma_product = sigma;
        v[i].token_id      = (i % 7 == 6) ? (1000 + i + 10000) : (1000 + i);
        v[i].p_target      = 0.9f;
    }
    cos_v119_stats_t s;
    cos_v119_simulate(&c, d, v, n, &s);
    char js[512];
    cos_v119_stats_to_json(&s, n, js, sizeof js);
    puts(js);
    free(d); free(v);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v119_self_test();
    if (!strcmp(argv[1], "--simulate") && argc == 4) {
        return cmd_simulate(atoi(argv[2]), (float)atof(argv[3]));
    }
    if (!strcmp(argv[1], "--gamma") && argc == 3) {
        cos_v119_config_t c;
        cos_v119_config_defaults(&c);
        printf("%d\n", cos_v119_adaptive_gamma(&c, (float)atof(argv[2])));
        return 0;
    }
    return usage(argv[0]);
}
