/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v126 σ-Embed CLI — self-test + deterministic embed + cosine probe
 * for the merge-gate smoke.
 */
#include "embed.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --cosine \"TEXT_A\" SIGMA_A \"TEXT_B\" SIGMA_B\n"
        "      # σ here is the uniform channel value (0..1)\n"
        "  %s --rank-demo\n"
        "      # 3-memory rank showing σ-weighted top-k\n",
        argv0, argv0, argv0);
    return 2;
}

static void sigma_uniform(cos_v126_sigma_profile_t *p, float v) {
    float ch[COS_V126_SIGMA_DIM];
    for (int i = 0; i < COS_V126_SIGMA_DIM; ++i) ch[i] = v;
    cos_v126_sigma_fill(p, ch);
}

static int cmd_cosine(int argc, char **argv) {
    if (argc != 6) return usage(argv[0]);
    cos_v126_config_t cfg; cos_v126_config_defaults(&cfg);
    cos_v126_sigma_profile_t sa, sb;
    sigma_uniform(&sa, (float)atof(argv[3]));
    sigma_uniform(&sb, (float)atof(argv[5]));
    cos_v126_embedding_t ea, eb;
    cos_v126_embed(&cfg, argv[2], &sa, &ea);
    cos_v126_embed(&cfg, argv[4], &sb, &eb);
    float c = cos_v126_cosine(&ea, &eb);
    float s = cos_v126_score(&cfg, &ea, &eb, &sb);
    printf("{\"dim\":%d,\"sigma_weight\":%.4f,\"lambda\":%.4f,"
           "\"cosine\":%.6f,\"score\":%.6f,\"sigma_product_b\":%.4f}\n",
           COS_V126_EMBED_DIM,
           (double)cfg.sigma_weight,
           (double)cfg.memory_uncertainty_penalty,
           (double)c, (double)s, (double)sb.sigma_product);
    return 0;
}

static int cmd_rank_demo(void) {
    cos_v126_config_t cfg; cos_v126_config_defaults(&cfg);
    cos_v126_sigma_profile_t sq, s0, s1, s2;
    sigma_uniform(&sq, 0.30f);
    sigma_uniform(&s0, 0.10f);
    sigma_uniform(&s1, 0.85f);
    sigma_uniform(&s2, 0.05f);
    cos_v126_embedding_t q, m0, m1, m2;
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &sq, &q);
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &s0, &m0);
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &s1, &m1);
    cos_v126_embed(&cfg, "quantum chromodynamics confinement scale",
                   &s2, &m2);
    cos_v126_embedding_t mems[3] = { m0, m1, m2 };
    cos_v126_sigma_profile_t ms[3] = { s0, s1, s2 };
    int idx[3]; float sc[3];
    int n = cos_v126_rank_topk(&cfg, &q, mems, ms, 3, idx, sc, 3);
    printf("{\"k\":%d,\"ranking\":[", n);
    for (int i = 0; i < n; ++i) {
        printf("%s{\"index\":%d,\"score\":%.6f,\"sigma_product\":%.4f}",
               i == 0 ? "" : ",",
               idx[i], (double)sc[i], (double)ms[idx[i]].sigma_product);
    }
    printf("]}\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test"))    return cos_v126_self_test();
    if (!strcmp(argv[1], "--cosine"))       return cmd_cosine(argc, argv);
    if (!strcmp(argv[1], "--rank-demo"))    return cmd_rank_demo();
    return usage(argv[0]);
}
