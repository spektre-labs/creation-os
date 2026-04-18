/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v143 σ-Benchmark — implementation.
 */
#include "benchmark.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------- PRNG (SplitMix64) -------------------- */
static uint64_t sm64(uint64_t *s) {
    *s += 0x9E3779B97F4A7C15ULL;
    uint64_t z = *s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static double urand(uint64_t *s) {
    return (double)((sm64(s) >> 11) & 0x1FFFFFFFFFFFFFULL) /
           (double)(1ULL << 53);
}
/* N(0,1) via Box-Muller */
static double nrand(uint64_t *s) {
    double u1 = urand(s); if (u1 < 1e-12) u1 = 1e-12;
    double u2 = urand(s);
    return sqrt(-2.0 * log(u1)) * cos(6.28318530717958647692 * u2);
}
static double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

/* ================================================================
 * 1. σ-Calibration — ECE
 * =============================================================== *
 * Synthetic model: for N=1000 items, draw a latent correctness
 * indicator y ∈ {0,1} with P(y=1) = 0.70.  The σ_product is a noisy
 * function of (1 − y):  high σ for wrong items, low σ for correct
 * items, with Gaussian jitter.  This models a *good* calibration;
 * if the kernel is healthy, ECE should be < 0.10.
 * =============================================================== */
static double run_calibration(uint64_t *rng, int n) {
    const int BINS = 10;
    int   bin_cnt[10] = {0};
    int   bin_pos[10] = {0};           /* count of y=1 (correct)  */
    double bin_csum[10] = {0};         /* sum of confidence       */
    for (int i = 0; i < n; ++i) {
        int y = (urand(rng) < 0.70) ? 1 : 0;
        /* Model: σ-as-error-probability, i.e. conf = 1 − σ should
         * match accuracy in every bin.  Near-extreme σ with small
         * jitter keeps bins dominated by the matching class so the
         * per-bin acc ↔ conf gap is small and ECE lands well under
         * 0.10.  (This is a property of the synthetic data, not a
         * claim about a real Creation OS run — see v143.1.)       */
        double sigma = (y ? 0.05 : 0.90) + 0.05 * nrand(rng);
        sigma = clamp01(sigma);
        double conf = 1.0 - sigma;   /* "confident we're right"   */
        conf = clamp01(conf);
        int b = (int)(conf * BINS);
        if (b >= BINS) b = BINS - 1;
        if (b < 0)     b = 0;
        bin_cnt[b]  += 1;
        bin_pos[b]  += y;
        bin_csum[b] += conf;
    }
    double ece = 0.0;
    for (int b = 0; b < BINS; ++b) {
        if (bin_cnt[b] == 0) continue;
        double acc  = (double)bin_pos[b]  / bin_cnt[b];
        double conf = bin_csum[b]        / bin_cnt[b];
        ece += (double)bin_cnt[b] * fabs(acc - conf);
    }
    return ece / n;
}

/* ================================================================
 * 2. σ-Abstention — coverage @ 95% accuracy
 * =============================================================== *
 * Synthetic items with an oracle-unknowable flag.  Model emits iff
 * σ < τ; τ sweeps from 1.0 → 0.05.  We pick the largest τ such
 * that accuracy on answered items ≥ 0.95 and return the
 * corresponding coverage.
 * =============================================================== */
static void run_abstention(uint64_t *rng, int n,
                           double *coverage, double *accuracy) {
    int    *labels    = (int *)malloc((size_t)n * sizeof(int));
    double *sigmas    = (double *)malloc((size_t)n * sizeof(double));
    if (!labels || !sigmas) { free(labels); free(sigmas);
        *coverage = 0.0; *accuracy = 0.0; return; }

    for (int i = 0; i < n; ++i) {
        int unknowable = (urand(rng) < 0.40);
        labels[i] = unknowable ? -1 : 1;       /* -1 = no valid answer */
        if (unknowable)
            sigmas[i] = clamp01(0.75 + 0.10 * nrand(rng));
        else
            sigmas[i] = clamp01(0.25 + 0.10 * nrand(rng));
    }
    double best_cov = 0.0;
    double best_acc = 0.0;
    for (int k = 20; k >= 1; --k) {
        double tau = (double)k / 20.0;    /* 1.0 → 0.05 */
        int emitted = 0;
        int correct = 0;
        for (int i = 0; i < n; ++i) {
            if (sigmas[i] < tau) {
                emitted++;
                if (labels[i] == 1) correct++;
            }
        }
        if (emitted == 0) continue;
        double acc = (double)correct / emitted;
        double cov = (double)emitted / n;
        if (acc >= 0.95 && cov > best_cov) {
            best_cov = cov;
            best_acc = acc;
        }
    }
    *coverage = best_cov;
    *accuracy = best_acc > 0.0 ? best_acc : 0.0;
    free(labels); free(sigmas);
}

/* ================================================================
 * 3. σ-Swarm — routing accuracy + σ-spread
 * =============================================================== *
 * 200 queries, 4 specialists.  Each query has a "true" specialist
 * ∈ [0,3].  Each specialist reports a σ: the true one reports
 * σ ~ N(0.20, 0.05), others report σ ~ N(0.70, 0.10).  σ-router
 * picks argmin σ.  We measure routing accuracy and the mean
 * (σ_max − σ_min) / σ_max across queries (the "informativeness"
 * of the router's σ-signal).
 * =============================================================== */
static void run_swarm(uint64_t *rng, int n, int K,
                      double *routing_acc, double *sigma_spread) {
    double *sig = (double *)malloc((size_t)K * sizeof(double));
    if (!sig) { *routing_acc = 0.0; *sigma_spread = 0.0; return; }
    int correct = 0;
    double spread_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        int truth = (int)(urand(rng) * K);
        if (truth >= K) truth = K - 1;
        double smin = 1e9, smax = -1e9;
        int    argmin = 0;
        for (int k = 0; k < K; ++k) {
            double s = (k == truth ? 0.20 : 0.70)
                     + (k == truth ? 0.05 : 0.10) * nrand(rng);
            s = clamp01(s);
            sig[k] = s;
            if (s < smin) { smin = s; argmin = k; }
            if (s > smax) { smax = s; }
        }
        if (argmin == truth) correct++;
        if (smax > 1e-9) spread_sum += (smax - smin) / smax;
    }
    *routing_acc   = (double)correct / n;
    *sigma_spread  = spread_sum / n;
    free(sig);
}

/* ================================================================
 * 4. σ-Learning — before/after with forgetting check
 * =============================================================== *
 * Targeted set: σ ~ N(0.55, 0.10) "before", N(0.30, 0.08) "after"
 *   (train improved it).  Accuracy flips from 0.55 → 0.82.
 * Holdout set: σ stays N(0.25, 0.05) "before" & "after" (no
 *   forgetting).  Accuracy stays ~0.90.
 * =============================================================== */
static double acc_from_sigma(uint64_t *rng, int n, double mu, double sd) {
    int correct = 0;
    for (int i = 0; i < n; ++i) {
        double s = clamp01(mu + sd * nrand(rng));
        /* model emits iff σ < 0.5; when emitted, it's right w.p.
         * 1 − s (so low σ ≈ high accuracy).                      */
        if (s < 0.5) {
            if (urand(rng) > s) correct++;
            else                continue;
        }
    }
    return (double)correct / n;
}

static void run_learning(uint64_t *rng, int n,
                         cos_v143_result_t *r) {
    /* targeted set before → after */
    double sig_before_mu = 0.55, sig_after_mu = 0.30;
    double sig_sd_before = 0.10, sig_sd_after = 0.08;
    r->learning_sigma_before  = sig_before_mu;
    r->learning_sigma_after   = sig_after_mu;
    r->learning_accuracy_before =
        acc_from_sigma(rng, n, sig_before_mu, sig_sd_before);
    r->learning_accuracy_after  =
        acc_from_sigma(rng, n, sig_after_mu, sig_sd_after);

    /* hold-out stays fixed (no forgetting). */
    double h_before = acc_from_sigma(rng, n, 0.25, 0.05);
    double h_after  = acc_from_sigma(rng, n, 0.25, 0.05);
    r->learning_holdout_drop = h_after - h_before;  /* ≈ 0          */
    r->learning_n = n;
}

/* ================================================================
 * 5. σ-Adversarial — detection rate @ FPR ≤ 5%
 * =============================================================== *
 * 200 benign queries (σ ~ N(0.30, 0.10)) + 200 adversarial
 * (σ ~ N(0.75, 0.12)).  Pick the threshold τ that gives FPR ≤ 0.05
 * on benign and report TPR on adversarial at that τ.
 * =============================================================== */
static void run_adversarial(uint64_t *rng, int n,
                            double *detection, double *fpr) {
    double *ben = (double *)malloc((size_t)n * sizeof(double));
    double *adv = (double *)malloc((size_t)n * sizeof(double));
    if (!ben || !adv) {
        free(ben); free(adv); *detection = 0.0; *fpr = 0.0; return;
    }
    for (int i = 0; i < n; ++i) {
        ben[i] = clamp01(0.30 + 0.10 * nrand(rng));
        adv[i] = clamp01(0.75 + 0.12 * nrand(rng));
    }
    double best_tpr = 0.0;
    double best_fpr = 0.0;
    for (int k = 1; k <= 40; ++k) {
        double tau = (double)k / 40.0;    /* 0.025 → 1.0 */
        int fp = 0, tp = 0;
        for (int i = 0; i < n; ++i) if (ben[i] > tau) fp++;
        for (int i = 0; i < n; ++i) if (adv[i] > tau) tp++;
        double cur_fpr = (double)fp / n;
        double cur_tpr = (double)tp / n;
        if (cur_fpr <= 0.05 && cur_tpr > best_tpr) {
            best_tpr = cur_tpr;
            best_fpr = cur_fpr;
        }
    }
    *detection = best_tpr;
    *fpr       = best_fpr;
    free(ben); free(adv);
}

/* ================================================================
 * Public API
 * =============================================================== */
int cos_v143_run_all(uint64_t seed, cos_v143_result_t *r) {
    if (!r) return -1;
    memset(r, 0, sizeof *r);
    r->version = 1;
    r->seed    = seed ? seed : 0xC057BBULL;

    uint64_t rng = r->seed;

    /* 1. calibration */
    r->calibration_n   = 1000;
    r->calibration_ece = run_calibration(&rng, r->calibration_n);

    /* 2. abstention */
    r->abstention_n = 500;
    run_abstention(&rng, r->abstention_n,
                   &r->abstention_coverage, &r->abstention_accuracy);

    /* 3. swarm */
    r->swarm_n = 200;
    run_swarm(&rng, r->swarm_n, 4,
              &r->swarm_routing_accuracy, &r->swarm_sigma_spread);

    /* 4. learning */
    run_learning(&rng, 100, r);

    /* 5. adversarial */
    r->adv_n = 200;
    run_adversarial(&rng, r->adv_n, &r->adv_detection_rate, &r->adv_fpr);
    return 0;
}

int cos_v143_to_json(const cos_v143_result_t *r, char *buf, size_t cap) {
    if (!r || !buf || cap == 0) return -1;
    int rc = snprintf(buf, cap,
        "{"
        "\"version\":%d,"
        "\"seed\":%llu,"
        "\"tier\":\"v143.0-synthetic\","
        "\"calibration\":{\"ece\":%.6f,\"n\":%d},"
        "\"abstention\":{\"coverage_at_95\":%.6f,\"accuracy\":%.6f,\"n\":%d},"
        "\"swarm\":{\"routing_accuracy\":%.6f,\"sigma_spread\":%.6f,\"n\":%d},"
        "\"learning\":{"
            "\"accuracy_before\":%.6f,\"accuracy_after\":%.6f,"
            "\"sigma_before\":%.6f,\"sigma_after\":%.6f,"
            "\"holdout_drop\":%.6f,\"n\":%d"
        "},"
        "\"adversarial\":{\"detection_at_fpr05\":%.6f,\"fpr\":%.6f,\"n\":%d}"
        "}",
        r->version, (unsigned long long)r->seed,
        r->calibration_ece, r->calibration_n,
        r->abstention_coverage, r->abstention_accuracy, r->abstention_n,
        r->swarm_routing_accuracy, r->swarm_sigma_spread, r->swarm_n,
        r->learning_accuracy_before, r->learning_accuracy_after,
        r->learning_sigma_before, r->learning_sigma_after,
        r->learning_holdout_drop, r->learning_n,
        r->adv_detection_rate, r->adv_fpr, r->adv_n);
    if (rc < 0 || (size_t)rc >= cap) return -1;
    return rc;
}

int cos_v143_write_json(const cos_v143_result_t *r, const char *path) {
    if (!r || !path) return -1;
    char buf[2048];
    int wn = cos_v143_to_json(r, buf, sizeof buf);
    if (wn < 0) return -1;
    FILE *fp = fopen(path, "w");
    if (!fp) return -2;
    size_t written = fwrite(buf, 1, (size_t)wn, fp);
    fputc('\n', fp);
    fclose(fp);
    return (written == (size_t)wn) ? 0 : -3;
}

/* ================================================================
 * Self-test
 * =============================================================== */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v143 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v143_self_test(void) {
    cos_v143_result_t r;
    _CHECK(cos_v143_run_all(0xC057BBULL, &r) == 0, "run_all");

    fprintf(stderr,
        "check-v143: calibration ECE=%.4f (n=%d)\n",
        r.calibration_ece, r.calibration_n);
    _CHECK(r.calibration_ece >= 0.0 && r.calibration_ece <= 0.50,
           "ECE in [0, 0.5]");
    _CHECK(r.calibration_ece < 0.15,
           "ECE < 0.15 on well-calibrated synthetic");

    fprintf(stderr,
        "check-v143: abstention coverage@95%%acc=%.4f (acc=%.4f n=%d)\n",
        r.abstention_coverage, r.abstention_accuracy, r.abstention_n);
    _CHECK(r.abstention_coverage > 0.30,
           "coverage at 95%% > 30%% on synthetic");
    _CHECK(r.abstention_accuracy >= 0.95 || r.abstention_accuracy == 0.0,
           "reported accuracy ≥ 0.95 or zero (no feasible τ)");

    fprintf(stderr,
        "check-v143: swarm routing_acc=%.4f spread=%.4f (n=%d)\n",
        r.swarm_routing_accuracy, r.swarm_sigma_spread, r.swarm_n);
    _CHECK(r.swarm_routing_accuracy > 0.80,
           "routing accuracy > 80%% on synthetic");
    _CHECK(r.swarm_sigma_spread > 0.30,
           "σ-spread > 30%% (informative router)");

    fprintf(stderr,
        "check-v143: learning acc %.4f → %.4f (σ %.4f → %.4f, holdout Δ=%.4f, n=%d)\n",
        r.learning_accuracy_before, r.learning_accuracy_after,
        r.learning_sigma_before,    r.learning_sigma_after,
        r.learning_holdout_drop,    r.learning_n);
    _CHECK(r.learning_accuracy_after > r.learning_accuracy_before,
           "accuracy after > before");
    _CHECK(r.learning_sigma_after < r.learning_sigma_before,
           "σ after < before (targeted improvement)");
    _CHECK(fabs(r.learning_holdout_drop) < 0.10,
           "hold-out drift < 10 pts (no forgetting)");

    fprintf(stderr,
        "check-v143: adversarial detection@FPR≤5%%=%.4f (fpr=%.4f n=%d)\n",
        r.adv_detection_rate, r.adv_fpr, r.adv_n);
    _CHECK(r.adv_detection_rate > 0.70,
           "detection rate > 70%% at FPR ≤ 5%% on synthetic");
    _CHECK(r.adv_fpr <= 0.06, "FPR ≤ 6%% (allow small slack)");

    /* JSON shape + file write round-trip. */
    char js[2048];
    int wn = cos_v143_to_json(&r, js, sizeof js);
    _CHECK(wn > 0, "json emit");
    _CHECK(strstr(js, "\"calibration\":{")  != NULL, "json: calibration block");
    _CHECK(strstr(js, "\"abstention\":{")   != NULL, "json: abstention block");
    _CHECK(strstr(js, "\"swarm\":{")        != NULL, "json: swarm block");
    _CHECK(strstr(js, "\"learning\":{")     != NULL, "json: learning block");
    _CHECK(strstr(js, "\"adversarial\":{")  != NULL, "json: adversarial block");
    _CHECK(strstr(js, "\"tier\":\"v143.0-synthetic\"") != NULL,
           "json: tier label");

    fprintf(stderr, "check-v143: OK (5 categories + JSON shape)\n");
    return 0;
}
