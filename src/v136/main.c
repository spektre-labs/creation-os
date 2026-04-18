/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v136 σ-Evolve — CLI wrapper.
 */
#include "evolve.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v136_evolve --self-test\n"
        "  creation_os_v136_evolve --demo [--seed N] [--out path.toml]\n"
        "  creation_os_v136_evolve --run --pop P --mu M --gens G "
            "[--seed N] [--n 200] [--out path.toml]\n");
    return 2;
}

static int cmd_run(int argc, char **argv, int want_demo) {
    cos_v136_cfg_t cfg; cos_v136_cfg_defaults(&cfg);
    int n = 200;
    const char *out_path = NULL;
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "--seed") && i + 1 < argc)
            cfg.seed = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--pop")  && i + 1 < argc) cfg.pop_size    = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--mu")   && i + 1 < argc) cfg.mu          = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--gens") && i + 1 < argc) cfg.generations = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--n")    && i + 1 < argc) n               = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out")  && i + 1 < argc) out_path        = argv[++i];
    }
    (void)want_demo;
    if (n <= 0 || n > 1024) n = 200;

    cos_v136_sample_t *data = (cos_v136_sample_t *)malloc(sizeof *data * n);
    if (!data) return 1;
    cos_v136_synthetic(data, n, cfg.seed ? cfg.seed : 1337);

    cos_v136_genome_t uniform;
    for (int i = 0; i < COS_V136_CHANNELS; ++i) uniform.w[i] = 1.0f;
    uniform.tau = 0.5f;
    float base_fit = cos_v136_evaluate(&uniform, data, n);

    cos_v136_genome_t best;
    float traj[256];
    int g = cfg.generations > 256 ? 256 : cfg.generations;
    cfg.generations = g;
    cos_v136_run(&cfg, data, n, &best, traj);

    printf("baseline_fitness=%.4f evolved_fitness=%.4f margin=%.4f\n",
           (double)base_fit, (double)best.fitness,
           (double)(best.fitness - base_fit));
    printf("tau=%.4f weights=", (double)best.tau);
    for (int i = 0; i < COS_V136_CHANNELS; ++i)
        printf("%.4f%s", (double)best.w[i],
               i + 1 < COS_V136_CHANNELS ? "," : "\n");
    printf("trajectory=");
    for (int i = 0; i < g; ++i)
        printf("%.4f%s", (double)traj[i], i + 1 < g ? "," : "\n");
    if (out_path && cos_v136_write_toml(&best, out_path) == 0)
        printf("wrote %s\n", out_path);
    free(data);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) return cos_v136_self_test();
    if (!strcmp(argv[1], "--demo"))      return cmd_run(argc - 2, argv + 2, 1);
    if (!strcmp(argv[1], "--run"))       return cmd_run(argc - 2, argv + 2, 0);
    return usage();
}
