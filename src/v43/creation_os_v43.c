/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_v43 — σ-guided knowledge distillation lab (weighted KL + curriculum + ensemble + calibration)
 *
 * Not merge-gate. Evidence class: lab math (C). No in-tree teacher/student training loop or API-backed KD.
 */
#include "calibrated_student.h"
#include "multi_teacher.h"
#include "progressive_distill.h"
#include "sigma_distill.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg)
{
    fprintf(stderr, "[v43 self-test] FAIL %s\n", msg);
    return 1;
}

static int test_sigma_weight(void)
{
    sigma_decomposed_t s;
    memset(&s, 0, sizeof s);
    s.epistemic = 0.8f;
    s.aleatoric = 0.1f;
    if (fabsf(v43_sigma_weight(&s) - 0.0f) > 1e-5f) {
        return fail("sigma_weight high epistemic should be 0");
    }
    s.epistemic = 0.1f;
    s.aleatoric = 0.2f;
    if (fabsf(v43_sigma_weight(&s) - 1.0f) > 1e-5f) {
        return fail("sigma_weight confident teacher should be 1");
    }
    s.epistemic = 0.25f;
    s.aleatoric = 0.6f;
    if (fabsf(v43_sigma_weight(&s) - 0.5f) > 1e-5f) {
        return fail("sigma_weight ambiguous teacher should be 0.5");
    }
    s.epistemic = 0.5f;
    s.aleatoric = 0.2f;
    if (fabsf(v43_sigma_weight(&s) - (-0.3f)) > 1e-5f) {
        return fail("sigma_weight danger band should be -0.3");
    }
    fprintf(stderr, "[v43 self-test] OK sigma_weight\n");
    return 0;
}

static int test_kl_and_distill(void)
{
    enum { n = 4 };
    float tlog[n] = {1.0f, 0.0f, 0.0f, 0.0f};
    float slog[n] = {1.0f, 0.0f, 0.0f, 0.0f};
    float kl0 = v43_kl_forward_pt_qs(tlog, slog, n, 1.0f);
    if (kl0 > 1e-3f) {
        return fail("KL identical logits should be ~0");
    }
    float slog2[n] = {0.0f, 1.0f, 0.0f, 0.0f};
    float kl1 = v43_kl_forward_pt_qs(tlog, slog2, n, 1.0f);
    if (kl1 <= 0.0f || !isfinite(kl1)) {
        return fail("KL different logits should be positive finite");
    }
    sigma_decomposed_t conf;
    memset(&conf, 0, sizeof conf);
    conf.epistemic = 0.1f;
    conf.aleatoric = 0.2f;
    float Lpos = v43_sigma_distillation_loss(slog2, tlog, &conf, n, 1.0f);
    if (Lpos <= 0.0f || !isfinite(Lpos)) {
        return fail("positive-weight distill loss should be > 0");
    }
    sigma_decomposed_t danger;
    memset(&danger, 0, sizeof danger);
    danger.epistemic = 0.5f;
    danger.aleatoric = 0.2f;
    float w = v43_sigma_weight(&danger);
    float Lneg = v43_sigma_distillation_loss(slog2, tlog, &danger, n, 1.0f);
    float rkl = v43_kl_reverse_qt_ps(tlog, slog2, n, 1.0f);
    if (fabsf(Lneg - w * rkl) > 1e-4f) {
        return fail("negative-weight branch should match w * reverse_KL");
    }
    fprintf(stderr, "[v43 self-test] OK KL + sigma_distillation_loss\n");
    return 0;
}

static int test_progressive(void)
{
    if (v43_stage_index_for_teacher_epistemic(0.1f) != 0) {
        return fail("stage 0 band");
    }
    if (v43_stage_index_for_teacher_epistemic(0.35f) != 1) {
        return fail("stage 1 band");
    }
    if (v43_stage_index_for_teacher_epistemic(0.6f) != 2) {
        return fail("stage 2 band");
    }
    if (v43_stage_index_for_teacher_epistemic(0.9f) != 3) {
        return fail("stage 3 band");
    }
    const v43_distill_stage_t *st = v43_stage_for_teacher_epistemic(0.85f);
    if (st->stage != 4 || st->learning_rate != 0.0f) {
        return fail("stage 4 should have LR 0 (self-play handoff)");
    }
    fprintf(stderr, "[v43 self-test] OK progressive stages\n");
    return 0;
}

static int test_ensemble(void)
{
    enum { n = 3 };
    float a[n] = {1.0f, 0.0f, 0.0f};
    float b[n] = {0.0f, 1.0f, 0.0f};
    v43_teacher_output_t ts[2];
    memset(ts, 0, sizeof ts);
    ts[0].logits = a;
    ts[0].sigma.epistemic = 0.1f;
    ts[0].sigma.aleatoric = 0.2f;
    ts[0].name = "A";
    ts[1].logits = b;
    ts[1].sigma.epistemic = 0.5f;
    ts[1].sigma.aleatoric = 0.2f;
    ts[1].name = "B";
    float out[n];
    if (v43_ensemble_teacher_logits(ts, 2, n, out) != 0) {
        return fail("ensemble should succeed");
    }
    float w0 = 1.0f / (0.1f + 0.1f);
    float w1 = 1.0f / (0.5f + 0.1f);
    float tw = w0 + w1;
    float e0 = (w0 * 1.0f + w1 * 0.0f) / tw;
    if (fabsf(out[0] - e0) > 1e-4f) {
        return fail("ensemble index 0 mismatch");
    }
    fprintf(stderr, "[v43 self-test] OK multi-teacher ensemble\n");
    return 0;
}

static int test_calibration(void)
{
    sigma_decomposed_t stu = {0};
    sigma_decomposed_t tea = {0};
    stu.epistemic = 0.4f;
    stu.aleatoric = 0.3f;
    tea.epistemic = 0.2f;
    tea.aleatoric = 0.5f;
    float c = v43_calibration_loss(&stu, &tea);
    float expect = (0.4f - 0.2f) * (0.4f - 0.2f) + (0.3f - 0.5f) * (0.3f - 0.5f);
    if (fabsf(c - expect) > 1e-5f) {
        return fail("calibration_loss mismatch");
    }
    float total = v43_student_total_loss(0.5f, c, 0.25f);
    if (fabsf(total - (0.5f + 0.25f * c)) > 1e-5f) {
        return fail("student_total_loss mismatch");
    }
    fprintf(stderr, "[v43 self-test] OK calibration + total loss\n");
    return 0;
}

static int self_test(void)
{
    if (test_sigma_weight() != 0) {
        return 1;
    }
    if (test_kl_and_distill() != 0) {
        return 1;
    }
    if (test_progressive() != 0) {
        return 1;
    }
    if (test_ensemble() != 0) {
        return 1;
    }
    if (test_calibration() != 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    if (argv[1] && strcmp(argv[1], "--self-test") == 0) {
        return self_test();
    }
    fprintf(stderr, "creation_os_v43: pass --self-test\n");
    return 2;
}
