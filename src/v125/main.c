/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v125 σ-DPO CLI — self-test, DPO loss probe, synthetic label +
 * distribution stats for the merge-gate smoke.
 */
#include "dpo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --loss BETA LP_CH LP_RE LPREF_CH LPREF_RE\n"
        "      # prints: {\"loss\":..., \"pref_prob\":...}\n"
        "  %s --label-synthetic\n"
        "      # emits {stats,pairs} JSON for a small fixed corpus\n"
        "  %s --collapse-demo\n"
        "      # prints before/after σ-distribution + verdict\n",
        argv0, argv0, argv0, argv0);
    return 2;
}

static int cmd_loss(int argc, char **argv) {
    if (argc != 7) return usage(argv[0]);
    cos_v125_logprobs_t lp;
    float beta = (float)atof(argv[2]);
    lp.logp_chosen       = (float)atof(argv[3]);
    lp.logp_rejected     = (float)atof(argv[4]);
    lp.logp_ref_chosen   = (float)atof(argv[5]);
    lp.logp_ref_rejected = (float)atof(argv[6]);
    float pp;
    float L = cos_v125_dpo_loss(beta, &lp, &pp);
    printf("{\"loss\":%.6f,\"pref_prob\":%.6f,\"beta\":%.4f}\n",
           (double)L, (double)pp, (double)beta);
    return 0;
}

static int cmd_label_synthetic(void) {
    cos_v125_config_t cfg; cos_v125_config_defaults(&cfg);
    cos_v125_interaction_t its[6];
    memset(its, 0, sizeof its);

    snprintf(its[0].prompt,      sizeof its[0].prompt,      "how many legs has an ant?");
    snprintf(its[0].context_key, sizeof its[0].context_key, "bio-ant");
    snprintf(its[0].response,    sizeof its[0].response,    "four");
    its[0].sigma_product = 0.80f; its[0].user_corrected = 1;
    snprintf(its[0].correction,  sizeof its[0].correction,  "six");

    snprintf(its[1].prompt,      sizeof its[1].prompt,      "capital of estonia v1");
    snprintf(its[1].context_key, sizeof its[1].context_key, "cap-ee");
    snprintf(its[1].response,    sizeof its[1].response,    "stockholm");
    its[1].sigma_product = 0.88f;

    snprintf(its[2].prompt,      sizeof its[2].prompt,      "capital of estonia v2");
    snprintf(its[2].context_key, sizeof its[2].context_key, "cap-ee");
    snprintf(its[2].response,    sizeof its[2].response,    "tallinn");
    its[2].sigma_product = 0.10f;

    its[3].sigma_product = 0.45f;                /* mid-σ → skipped */
    snprintf(its[3].context_key, sizeof its[3].context_key, "mid");

    snprintf(its[4].prompt,      sizeof its[4].prompt,      "moon mass");
    snprintf(its[4].context_key, sizeof its[4].context_key, "astro-moon");
    snprintf(its[4].response,    sizeof its[4].response,    "no idea");
    its[4].sigma_product = 0.91f;

    its[5].sigma_product = 0.05f;                /* low-σ, unpaired */
    snprintf(its[5].context_key, sizeof its[5].context_key, "solo-anchor");

    cos_v125_pair_t pairs[8];
    cos_v125_dataset_stats_t ds;
    int np = cos_v125_label_dataset(&cfg, its, 6, pairs, 8, &ds);

    char sjs[512];
    cos_v125_stats_to_json(&ds, &cfg, sjs, sizeof sjs);
    printf("{\"stats\":%s,\"pairs\":[", sjs);
    for (int i = 0; i < np; ++i) {
        const cos_v125_pair_t *p = &pairs[i];
        printf("%s{\"source\":%d,\"chosen\":\"%s\",\"rejected\":\"%s\","
               "\"sigma_chosen\":%.4f,\"sigma_rejected\":%.4f}",
               i == 0 ? "" : ",",
               (int)p->source, p->chosen, p->rejected,
               (double)p->sigma_chosen, (double)p->sigma_rejected);
    }
    printf("]}\n");
    return 0;
}

static int cmd_collapse_demo(void) {
    cos_v125_config_t cfg; cos_v125_config_defaults(&cfg);
    float before[32], after_ok[32], after_co[32];
    for (int i = 0; i < 32; ++i) {
        before[i]   = 0.05f + (float)i * 0.028f;
        after_ok[i] = before[i] + 0.01f * ((i % 2) ? 1.0f : -1.0f);
        after_co[i] = 0.50f + 0.002f * ((i % 2) ? 1.0f : -1.0f);
    }
    cos_v125_distribution_stats_t db, dok, dco;
    cos_v125_sigma_distribution(before,   32, &db);
    cos_v125_sigma_distribution(after_ok, 32, &dok);
    cos_v125_sigma_distribution(after_co, 32, &dco);
    char jb[256], jok[256], jco[256];
    cos_v125_distribution_to_json(&db,  jb,  sizeof jb);
    cos_v125_distribution_to_json(&dok, jok, sizeof jok);
    cos_v125_distribution_to_json(&dco, jco, sizeof jco);
    cos_v125_mode_t v_ok = cos_v125_check_mode_collapse(&cfg, &db, &dok);
    cos_v125_mode_t v_co = cos_v125_check_mode_collapse(&cfg, &db, &dco);
    printf("{\"before\":%s,\"after_ok\":%s,\"after_collapse\":%s,"
           "\"verdict_ok\":\"%s\",\"verdict_collapse\":\"%s\"}\n",
           jb, jok, jco,
           v_ok == COS_V125_MODE_OK ? "ok" : "collapse",
           v_co == COS_V125_MODE_OK ? "ok" : "collapse");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test"))       return cos_v125_self_test();
    if (!strcmp(argv[1], "--loss"))            return cmd_loss(argc, argv);
    if (!strcmp(argv[1], "--label-synthetic")) return cmd_label_synthetic();
    if (!strcmp(argv[1], "--collapse-demo"))   return cmd_collapse_demo();
    return usage(argv[0]);
}
