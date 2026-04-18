/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v129 σ-Federated — CLI wrapper.
 */
#include "federated.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v129_federated --self-test\n"
        "  creation_os_v129_federated --aggregate \"σ0,σ1,...\" \"v0,v1,...\"\n"
        "  creation_os_v129_federated --dp-noise σ_node ε_budget\n"
        "  creation_os_v129_federated --adaptive-k σ_node\n");
    return 2;
}

static int parse_floats(const char *csv, float *out, int max) {
    int n = 0;
    const char *p = csv;
    while (*p && n < max) {
        char *end = NULL;
        float v = strtof(p, &end);
        if (end == p) break;
        out[n++] = v;
        p = end;
        while (*p == ',' || *p == ' ') p++;
    }
    return n;
}

static int cmd_aggregate(const char *sigmas_csv, const char *vec_csv) {
    enum { MAX_NODES = 16, MAX_DIM = 32 };
    float sigmas[MAX_NODES];
    int n = parse_floats(sigmas_csv, sigmas, MAX_NODES);
    if (n < 2) { fprintf(stderr, "need at least 2 σ values\n"); return 2; }

    /* each node's delta = constant vector with value = node_id+1 */
    float *vecs = (float*)calloc((size_t)n * MAX_DIM, sizeof(float));
    int   dim_count = parse_floats(vec_csv, vecs, MAX_DIM);
    if (dim_count < 1) { free(vecs); fprintf(stderr, "need values\n"); return 2; }
    /* Broadcast the single provided vector to each node (scaled by 1 / (i+1)) */
    for (int i = 1; i < n; ++i)
        for (int k = 0; k < dim_count; ++k)
            vecs[i*MAX_DIM + k] = vecs[k] / (float)(i + 1);

    cos_v129_node_delta_t nodes[MAX_NODES];
    for (int i = 0; i < n; ++i) {
        nodes[i].node_id     = i;
        nodes[i].sigma_node  = sigmas[i];
        nodes[i].delta       = &vecs[i*MAX_DIM];
        nodes[i].dim         = dim_count;
        nodes[i].epsilon_spent = 0.0f;
    }

    float out_delta[MAX_DIM];
    cos_v129_aggregate_stats_t stats;
    int rc = cos_v129_aggregate(nodes, n, out_delta, dim_count, &stats);
    if (rc < 0) { free(vecs); fprintf(stderr, "aggregate failed\n"); return 1; }

    char js[512];
    cos_v129_stats_to_json(&stats, js, sizeof js);
    printf("%s\n", js);
    printf("out_delta:");
    for (int k = 0; k < dim_count; ++k) printf(" %.6f", (double)out_delta[k]);
    printf("\n");
    free(vecs);
    return 0;
}

static int cmd_dp_noise(float sigma_node, float eps) {
    cos_v129_config_t cfg; cos_v129_config_defaults(&cfg);
    float probe[64] = {0};
    uint64_t rs = 0xABCDEFULL;
    float sig_dp = cos_v129_add_dp_noise(probe, 64, sigma_node, eps, &cfg, &rs);
    double m = 0, m2 = 0;
    for (int i = 0; i < 64; ++i) { m += probe[i]; m2 += (double)probe[i]*probe[i]; }
    m /= 64.0; m2 = m2 / 64.0 - m*m;
    printf("{\"sigma_node\":%.4f,\"epsilon_budget\":%.4f,\"sigma_dp\":%.6f,"
           "\"empirical_mean\":%.6f,\"empirical_var\":%.6f}\n",
           (double)sigma_node, (double)eps, (double)sig_dp, m, m2);
    return 0;
}

static int cmd_adaptive_k(float sigma_node) {
    cos_v129_config_t cfg; cos_v129_config_defaults(&cfg);
    int k = cos_v129_adaptive_k(&cfg, sigma_node);
    printf("{\"sigma_node\":%.4f,\"k\":%d,\"k_min\":%d,\"k_max\":%d}\n",
           (double)sigma_node, k, cfg.k_min, cfg.k_max);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) return cos_v129_self_test();
    if (!strcmp(argv[1], "--aggregate")) {
        if (argc < 4) return usage();
        return cmd_aggregate(argv[2], argv[3]);
    }
    if (!strcmp(argv[1], "--dp-noise")) {
        if (argc < 4) return usage();
        return cmd_dp_noise((float)atof(argv[2]), (float)atof(argv[3]));
    }
    if (!strcmp(argv[1], "--adaptive-k")) {
        if (argc < 3) return usage();
        return cmd_adaptive_k((float)atof(argv[2]));
    }
    return usage();
}
