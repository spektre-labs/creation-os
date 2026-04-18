/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v140 σ-Causal — CLI.
 */
#include "causal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --demo           8-channel attribution + counterfactual\n",
        a0, a0);
    return 2;
}

static int demo(void) {
    /* 1. counterfactual on a small world model ------------------
     * v139 fit needs n > D training pairs.  We generate 81 steps
     * (80 pairs) so the D=16 system is well over-determined.     */
    const int D = 16;
    const int Tp1 = 81;
    float *traj = (float *)malloc((size_t)Tp1 * D * sizeof(float));
    if (!traj) return 1;
    cos_v139_synthetic_trajectory(traj, D, Tp1, 1.00f, 99ULL);
    cos_v139_model_t m;
    if (cos_v139_fit(&m, D, Tp1 - 20, traj, traj + D) != 0) {
        free(traj); return 1;
    }
    cos_v140_counterfactual_t cf;
    cos_v140_counterfactual(&m, traj + 5 * D, 0, traj[5 * D] + 1.5f, &cf);

    /* 2. attribution on 8-channel σ-vector */
    float w[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    float sigma[8] = {
        0.30f, 0.92f, 0.35f, 0.28f, 0.40f, 0.20f, 0.25f, 0.55f
    };
    cos_v140_attribution_t attr;
    cos_v140_attribute(w, sigma, 8, 0.50f, &attr);

    char buf[1024];
    cos_v140_attribution_to_json(&attr, buf, sizeof buf);
    printf("{\"v140_demo\":true,"
           "\"counterfactual\":{\"index\":%d,\"delta_norm\":%.6f,"
           "\"sigma_causal\":%.6f},"
           "\"attribution\":%s}\n",
           cf.intervened_index, (double)cf.delta_norm,
           (double)cf.sigma_causal, buf);
    free(traj);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v140_self_test();
    if (!strcmp(argv[1], "--demo"))      return demo();
    return usage(argv[0]);
}
