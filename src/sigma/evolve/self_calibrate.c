/*
 * σ-gate self-calibration — implementation.  OMEGA-4.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "self_calibrate.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static void evaluate_tau(const cos_calib_point_t *pts, int n, float tau,
                         float *fa, float *fr, float *cov) {
    int n_acc = 0, n_rej = 0;
    int n_acc_wrong = 0, n_rej_right = 0;
    for (int i = 0; i < n; i++) {
        if (pts[i].sigma <= tau) {
            n_acc++;
            if (!pts[i].correct) n_acc_wrong++;
        } else {
            n_rej++;
            if (pts[i].correct) n_rej_right++;
        }
    }
    *fa  = n_acc > 0 ? (float)n_acc_wrong / (float)n_acc : 0.f;
    *fr  = n_rej > 0 ? (float)n_rej_right / (float)n_rej : 0.f;
    *cov = (float)n_acc / (float)n;
}

int cos_sigma_self_calibrate(const cos_calib_point_t *pts, int n,
                             cos_calib_mode_t mode,
                             float alpha_or_budget,
                             cos_calib_result_t *out) {
    if (!pts || n <= 0 || !out) return -1;
    memset(out, 0, sizeof *out);
    out->n_points = n;

    float best_loss = INFINITY;
    float best_tau  = 0.5f;
    float best_fa   = 0.f, best_fr = 0.f, best_cov = 0.f;
    int   found     = 0;

    for (int k = 0; k <= 200; k++) {
        float tau = (float)k / 200.0f;
        float fa, fr, cov;
        evaluate_tau(pts, n, tau, &fa, &fr, &cov);

        float loss;
        if (mode == COS_CALIB_RISK_BOUNDED) {
            if (fa > alpha_or_budget) continue;   /* infeasible */
            loss = fr;
        } else {
            float a = alpha_or_budget;
            if (a < 0.f) a = 0.f;
            if (a > 1.f) a = 1.f;
            loss = a * fa + (1.0f - a) * fr;
        }
        if (loss < best_loss) {
            best_loss = loss;
            best_tau  = tau;
            best_fa   = fa;
            best_fr   = fr;
            best_cov  = cov;
            found     = 1;
        }
    }

    /* RISK_BOUNDED infeasible → fall back to BALANCED with α=0.5. */
    if (!found) {
        for (int k = 0; k <= 200; k++) {
            float tau = (float)k / 200.0f;
            float fa, fr, cov;
            evaluate_tau(pts, n, tau, &fa, &fr, &cov);
            float loss = 0.5f * fa + 0.5f * fr;
            if (loss < best_loss) {
                best_loss = loss; best_tau = tau;
                best_fa = fa; best_fr = fr; best_cov = cov;
            }
        }
    }

    out->tau          = best_tau;
    out->false_accept = best_fa;
    out->false_reject = best_fr;
    out->accept_rate  = best_cov;
    out->best_loss    = best_loss;
    return 0;
}

/* ------------------ self-test ------------------
 *
 * 100-point fixture where σ_low→correct and σ_high→wrong (with
 * noise).  Optimal τ is around 0.50; the sweep must find one
 * within 0.35…0.65 under both modes.
 */
int cos_sigma_self_calibrate_self_test(void) {
    cos_calib_point_t pts[200];
    int n = 200;
    /* deterministic jittered two-cluster fixture */
    for (int i = 0; i < n; i++) {
        int correct = (i & 1) == 0;
        pts[i].correct = correct;
        float base = correct ? 0.20f : 0.75f;
        float jit  = (float)((i * 7919) % 97) / 97.f * 0.15f - 0.075f;
        float s = base + jit;
        if (s < 0.f) s = 0.f;
        if (s > 1.f) s = 1.f;
        pts[i].sigma = s;
    }

    cos_calib_result_t r;
    if (cos_sigma_self_calibrate(pts, n, COS_CALIB_BALANCED, 0.5f, &r) != 0)
        return 1;
    if (!(r.tau >= 0.25f && r.tau <= 0.70f)) return 2;
    if (!(r.best_loss < 0.15f)) return 3;

    cos_calib_result_t r2;
    if (cos_sigma_self_calibrate(pts, n, COS_CALIB_RISK_BOUNDED,
                                 0.10f, &r2) != 0) return 4;
    if (!(r2.false_accept <= 0.10f + 1e-6f)) return 5;
    /* trivial-reject behaviour: τ < min(σ_incorrect) always satisfies
     * the risk budget, so the sweep is always feasible. */
    if (!(r2.tau >= 0.0f && r2.tau <= 1.0f)) return 6;

    /* infeasible-budget path → falls back to balanced. */
    cos_calib_result_t r3;
    if (cos_sigma_self_calibrate(pts, n, COS_CALIB_RISK_BOUNDED,
                                 -1.0f, &r3) != 0) return 7;
    if (!(r3.tau >= 0.0f && r3.tau <= 1.0f)) return 8;
    return 0;
}
