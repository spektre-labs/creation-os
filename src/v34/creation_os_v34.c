/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v34 — σ decomposition + Platt calibration + extended channels (POSIX lab)
 *
 * Not merge-gate. Benchmark rows for TruthfulQA / FreshQA / SelfAware remain **N-tier**
 * until archived repro bundles exist (see docs/WHAT_IS_REAL.md).
 */
#if defined(_WIN32)
#include <stdio.h>
int main(void)
{
    fprintf(stderr, "creation_os_v34: POSIX-only.\n");
    return 2;
}
#else
#define _POSIX_C_SOURCE 200809L

#include "../sigma/calibrate.h"
#include "../sigma/channels.h"
#include "../sigma/channels_v34.h"
#include "../sigma/decompose.h"
#include "../v33/router.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int self_test_decompose(void)
{
    float flat[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    sigma_decomposed_t d0;
    sigma_decompose_dirichlet_evidence(flat, 8, &d0);
    float peaked[8] = { 8.f, -4.f, -4.f, -4.f, -4.f, -4.f, -4.f, -4.f };
    sigma_decomposed_t d1;
    sigma_decompose_dirichlet_evidence(peaked, 8, &d1);
    float h0 = sigma_logit_entropy(flat, 8);
    float h1 = sigma_logit_entropy(peaked, 8);
    if (!(h0 > h1)) {
        fprintf(stderr, "[v34 self-test] FAIL flat logits should have higher normalized entropy\n");
        return 1;
    }
    fprintf(stderr, "[v34 self-test] OK decompose (entropy flat=%.4f peaked=%.4f; epi flat=%.6f peaked=%.6f)\n",
        h0, h1, d0.epistemic, d1.epistemic);
    return 0;
}

static int self_test_platt(void)
{
    float p = sigma_platt_p_correct(0.5f, -2.0f, 0.1f);
    if (!(p > 0.0f && p < 1.0f)) {
        fprintf(stderr, "[v34 self-test] FAIL platt range\n");
        return 1;
    }
    sigma_platt_params_t pp;
    if (sigma_platt_load("config/sigma_calibration.json", &pp) != 0) {
        fprintf(stderr, "[v34 self-test] SKIP sigma_calibration.json missing\n");
        return 0;
    }
    fprintf(stderr, "[v34 self-test] OK platt (p=%.4f, calib a=%.4f b=%.4f valid=%d)\n", p, pp.a, pp.b,
        pp.valid);
    return 0;
}

static int self_test_channels_v34(void)
{
    float lp[8] = { 1.f, 0.2f, 0.1f, 0.f, -0.5f, -1.f, -1.f, -2.f };
    sigma_channels_v34_t st;
    sigma_channels_v34_fill_last_step(lp, 8, &st);
    if (!(st.top_margin >= 0.f && st.top_margin <= 1.f)) {
        fprintf(stderr, "[v34 self-test] FAIL top_margin range\n");
        return 1;
    }
    float nll[] = { 0.4f, 0.8f, 1.2f };
    float pe = sigma_predictive_entropy_mean(nll, 3);
    if (fabsf(pe - (0.4f + 0.8f + 1.2f) / 3.f) > 1e-4f) {
        fprintf(stderr, "[v34 self-test] FAIL predictive entropy mean\n");
        return 1;
    }
    int s0[] = { 1, 2, 3 };
    int s1[] = { 1, 2, 3 };
    int s2[] = { 9, 8, 7 };
    const int *rows[] = { s0, s1, s2 };
    float se = sigma_semantic_entropy_cluster_equal(rows, 3, 3);
    if (!(se >= 0.f && se <= 1.f)) {
        fprintf(stderr, "[v34 self-test] FAIL semantic entropy range\n");
        return 1;
    }
    float ent_tok[] = { 0.1f, 0.9f, 0.2f };
    unsigned char mask[] = { 0, 1, 1 };
    float ce = sigma_critical_token_entropy_weighted(ent_tok, mask, 3);
    if (fabsf(ce - (0.9f + 0.2f) / 2.f) > 1e-4f) {
        fprintf(stderr, "[v34 self-test] FAIL critical token entropy\n");
        return 1;
    }
    if (sigma_eigenscore_hidden_proxy(NULL, 4) != 0.f) {
        fprintf(stderr, "[v34 self-test] FAIL eigenscore stub NULL\n");
        return 1;
    }
    fprintf(stderr, "[v34 self-test] OK channels_v34\n");
    return 0;
}

static int self_test_router_v34(void)
{
    cos_routing_config_t cfg;
    cos_routing_defaults(&cfg);
    (void)cos_routing_load("config/routing.json", &cfg);
    cfg.routing_mode_v34 = 1;
    cfg.platt_calib_valid = 1;
    cfg.platt_a = -8.0f;
    cfg.platt_b = 0.0f;
    cfg.threshold_min_p_correct = 0.55f;
    cfg.threshold_aleatoric = 10.0f;

    float flat[8] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
    int budget = 8;
    cos_route_result_t r = cos_route_from_logits_v34(&cfg, flat, 8, 1, &budget);
    if (r.decision != COS_ROUTE_FALLBACK) {
        fprintf(stderr, "[v34 self-test] FAIL expected FALLBACK on flat logits (got %d)\n", (int)r.decision);
        return 1;
    }

    cos_routing_config_t cfg2 = cfg;
    cfg2.platt_calib_valid = 0;
    cfg2.threshold_epistemic_raw = 100.0f;
    cfg2.threshold_top_margin = 2.0f;
    cfg2.threshold_min_p_correct = 0.99f;
    cfg2.threshold_aleatoric = 0.0f;
    float peaked[8] = { 8.f, -4.f, -4.f, -4.f, -4.f, -4.f, -4.f, -4.f };
    budget = 8;
    cos_route_result_t r2 = cos_route_from_logits_v34(&cfg2, peaked, 8, 0, &budget);
    if (r2.decision != COS_ROUTE_PRIMARY_AMBIGUOUS) {
        fprintf(stderr, "[v34 self-test] FAIL expected ambiguous on peaked with aleatoric-only gate\n");
        return 1;
    }
    fprintf(stderr, "[v34 self-test] OK router_v34\n");
    return 0;
}

/** Concordance AUC for scores where higher => label 1 (lab smoke; not M-tier). */
static float toy_auc(const float *score, const int *y, int n)
{
    int pos = 0, neg = 0;
    for (int i = 0; i < n; i++) {
        if (y[i])
            pos++;
        else
            neg++;
    }
    if (pos == 0 || neg == 0)
        return 0.5f;
    double c = 0.0;
    for (int i = 0; i < n; i++) {
        if (!y[i])
            continue;
        for (int j = 0; j < n; j++) {
            if (y[j])
                continue;
            if (score[i] > score[j])
                c += 1.0;
            else if (score[i] == score[j])
                c += 0.5;
        }
    }
    return (float)(c / ((double)pos * (double)neg));
}

static int bench_synthetic_auroc(void)
{
    const int n = 64;
    float *score = (float *)malloc(sizeof(float) * (size_t)n);
    int *y = (int *)malloc(sizeof(int) * (size_t)n);
    if (!score || !y) {
        free(score);
        free(y);
        return 2;
    }
    for (int i = 0; i < n; i++) {
        float logits[8];
        for (int j = 0; j < 8; j++)
            logits[j] = (float)(i + j) * 0.05f - 2.0f * (float)(i % 3);
        sigma_decomposed_t d;
        sigma_decompose_dirichlet_evidence(logits, 8, &d);
        float ent = sigma_logit_entropy(logits, 8);
        score[i] = -d.epistemic - 0.25f * ent;
        y[i] = (ent < 0.55f) ? 1 : 0;
    }
    float auc = toy_auc(score, y, n);
    free(score);
    free(y);
    fprintf(stdout, "SYNTHETIC_AUROC epistemic_like_score vs low_entropy_label auc=%.4f (toy; not M-tier)\n", auc);
    return 0;
}

static void print_help(const char *argv0)
{
    printf("usage: %s [--self-test] [--bench-synthetic-auroc] [--help]\n", argv0);
    printf("\n");
    printf("v34 lab: Dirichlet-style σ decomposition + Platt map + extended channels.\n");
    printf("\n");
    printf("Not part of merge-gate.\n");
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test")) {
            int a = self_test_decompose();
            int b = self_test_platt();
            int c = self_test_channels_v34();
            int d = self_test_router_v34();
            return (a == 0 && b == 0 && c == 0 && d == 0) ? 0 : 2;
        }
        if (!strcmp(argv[i], "--bench-synthetic-auroc"))
            return bench_synthetic_auroc();
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_help(argv[0]);
            return 0;
        }
    }
    print_help(argv[0]);
    fprintf(stderr, "Hint: %s --self-test\n", argv[0]);
    return 2;
}
#endif
