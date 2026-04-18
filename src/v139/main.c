/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v139 σ-WorldModel — CLI.
 */
#include "world_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --demo          run a D=16 fit + rollout summary\n",
        argv0, argv0);
    return 2;
}

static int demo(void) {
    const int D   = 16;
    const int Tp1 = 401;
    float *traj = (float *)malloc((size_t)Tp1 * D * sizeof(float));
    if (!traj) return 1;
    cos_v139_synthetic_trajectory(traj, D, Tp1, 1.00f, 42ULL);
    int n_fit = 300;
    cos_v139_model_t m;
    if (cos_v139_fit(&m, D, n_fit, traj, traj + D) != 0) {
        free(traj); return 1;
    }
    char js[256];
    cos_v139_to_json(&m, js, sizeof js);
    printf("{\"v139_demo\":true,\"model\":%s,", js);

    cos_v139_rollout_t ro;
    cos_v139_rollout(&m, traj + (size_t)n_fit * D, 8,
                     traj + (size_t)n_fit * D, &ro);
    printf("\"rollout\":{\"horizon\":%d,\"sigma_world\":[", ro.horizon);
    for (int i = 0; i < ro.horizon; ++i)
        printf("%s%.6f", i ? "," : "", (double)ro.sigma_world[i]);
    printf("],\"monotone_rising\":%s}}\n",
           ro.monotone_rising ? "true" : "false");
    free(traj);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v139_self_test();
    if (!strcmp(argv[1], "--demo"))      return demo();
    return usage(argv[0]);
}
