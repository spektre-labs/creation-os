/* cos-coverage-curve — SCI-3.
 *
 * Sweeps τ ∈ [0,1] in configurable steps and for each τ reports the
 * selective-prediction trade-off:
 *
 *     coverage(τ)         = #{accepted} / #{scored}
 *     accuracy(τ)         = #{correct ∧ accepted} / #{accepted}
 *     accuracy_all(τ)     = #{correct} / #{scored}           (constant)
 *     empirical_risk(τ)   = 1 − accuracy(τ)
 *     risk_ucb(τ)         = Hoeffding UCB on empirical_risk
 *
 * Output is a compact JSON document whose shape is:
 *
 *   {
 *     "schema": "cos.coverage_curve.v1",
 *     "dataset": "...",
 *     "mode": "...",
 *     "delta": 0.05,
 *     "n_scored": 140,
 *     "accuracy_all": 0.336,
 *     "rows": [
 *        {"tau":0.00, "coverage":0.00, "accuracy":null, "risk_ucb":null},
 *        {"tau":0.05, "coverage":0.01, "accuracy":1.00, "risk_ucb":...},
 *         ...
 *     ]
 *   }
 *
 * The table is exactly the "accuracy vs coverage trade-off" that the
 * user-facing README paragraph promises: pick your τ, read off the
 * guaranteed accuracy at that coverage.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "conformal.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define COS_CC_MAX_SAMPLES 8192

static const char *DEFAULT_DATASET =
    "benchmarks/pipeline/truthfulqa_817_detail.jsonl";

static int ensure_parent_dir(const char *path) {
    char buf[1024];
    if (snprintf(buf, sizeof(buf), "%s", path) >= (int)sizeof(buf)) return -1;
    char *slash = strrchr(buf, '/');
    if (!slash) return 0;
    *slash = '\0';
    if (*buf == '\0') return 0;
    if (mkdir(buf, 0700) != 0 && errno != EEXIST) return -1;
    return 0;
}

static void usage(void) {
    fprintf(stderr,
        "cos-coverage-curve — accuracy-coverage sweep (SCI-3)\n"
        "\n"
        "  cos-coverage-curve [--dataset PATH]   (default: %s)\n"
        "                     [--mode MODE]       (default: pipeline)\n"
        "                     [--step  S]         (default: 0.05)\n"
        "                     [--delta D]         (default: 0.05)\n"
        "                     [--out   PATH]      (default: benchmarks/coverage_curve.json)\n"
        "                     [--self-test]\n"
        "\n"
        "Each row of the output JSON is (τ, coverage, accuracy_accepted,\n"
        "risk_ucb) so a downstream plot can show the classical selective-\n"
        "prediction trade-off at a glance.\n",
        DEFAULT_DATASET);
}

/* ISO format without locale surprises. */
static void fmt_float_or_null(char *out, size_t cap,
                              double value, int n_used) {
    if (n_used <= 0) snprintf(out, cap, "null");
    else             snprintf(out, cap, "%.6f", value);
}

int main(int argc, char **argv) {
    const char *dataset = DEFAULT_DATASET;
    const char *mode    = "pipeline";
    const char *outpath = "benchmarks/coverage_curve.json";
    float step  = 0.05f;
    float delta = 0.05f;
    int self_test = 0;

    for (int i = 1; i < argc; ++i) {
        if      (!strcmp(argv[i], "--dataset") && i + 1 < argc) dataset = argv[++i];
        else if (!strcmp(argv[i], "--mode")    && i + 1 < argc) mode    = argv[++i];
        else if (!strcmp(argv[i], "--out")     && i + 1 < argc) outpath = argv[++i];
        else if (!strcmp(argv[i], "--step")    && i + 1 < argc) step    = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--delta")   && i + 1 < argc) delta   = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--self-test")) self_test = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "cos-coverage-curve: unknown arg: %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    if (self_test) {
        int rc = cos_conformal_self_test();
        /* Also sweep a tiny synthetic set and verify coverage monotonicity. */
        if (rc == 0) {
            cos_conformal_sample_t s[100];
            for (int i = 0; i < 100; ++i) {
                s[i].sigma   = (float)i / 100.0f;
                s[i].correct = (s[i].sigma < 0.5f) ? 1 : 0;
                s[i].domain[0] = '\0';
            }
            float prev = -1.0f;
            for (float t = 0.0f; t <= 1.0f + 1e-6f; t += 0.1f) {
                cos_conformal_report_t r;
                if (cos_conformal_eval_at(s, 100, t, 0.10f, &r) != 0) { rc = -1; break; }
                if (r.coverage < prev - 1e-6f)                         { rc = -1; break; }
                prev = r.coverage;
            }
        }
        printf("cos-coverage-curve: self-test %s\n", rc == 0 ? "PASS" : "FAIL");
        return rc == 0 ? 0 : 1;
    }

    if (!(step > 0.0f && step < 1.0f)) {
        fprintf(stderr, "cos-coverage-curve: --step must be in (0,1)\n");
        return 2;
    }

    cos_conformal_sample_t *samples = (cos_conformal_sample_t *)
        calloc((size_t)COS_CC_MAX_SAMPLES, sizeof(*samples));
    if (!samples) { fprintf(stderr, "OOM\n"); return 1; }

    int n = cos_conformal_load_jsonl(dataset, mode, samples, COS_CC_MAX_SAMPLES);
    if (n < 0) {
        fprintf(stderr, "cos-coverage-curve: cannot read %s\n", dataset);
        free(samples);
        return 1;
    }

    int n_scored = 0, n_correct = 0;
    for (int i = 0; i < n; ++i) {
        if (samples[i].correct < 0) continue;
        n_scored  += 1;
        n_correct += (samples[i].correct > 0) ? 1 : 0;
    }
    double acc_all = (n_scored > 0)
        ? (double)n_correct / (double)n_scored : 0.0;

    if (ensure_parent_dir(outpath) != 0) {
        fprintf(stderr, "cos-coverage-curve: cannot create parent of %s\n", outpath);
        free(samples);
        return 1;
    }
    FILE *fp = fopen(outpath, "w");
    if (!fp) {
        fprintf(stderr, "cos-coverage-curve: cannot open %s\n", outpath);
        free(samples);
        return 1;
    }

    fprintf(fp,
        "{\n"
        "  \"schema\": \"cos.coverage_curve.v1\",\n"
        "  \"dataset\": \"%s\",\n"
        "  \"mode\": \"%s\",\n"
        "  \"delta\": %.6f,\n"
        "  \"step\": %.6f,\n"
        "  \"n_scored\": %d,\n"
        "  \"n_correct\": %d,\n"
        "  \"accuracy_all\": %.6f,\n"
        "  \"rows\": [\n",
        dataset, mode, delta, step, n_scored, n_correct, acc_all);

    /* Build the τ grid with stable float loop: integer step count. */
    int n_steps = (int)((1.0f + 0.5f * step) / step) + 1;
    if (n_steps < 2) n_steps = 2;
    if (n_steps > 4096) n_steps = 4096;

    printf("  %-6s %-10s %-18s %-10s %-10s\n",
           "τ", "coverage", "accuracy_accepted", "risk_ucb", "n_accept");

    for (int k = 0; k < n_steps; ++k) {
        float tau = (float)k * step;
        if (tau > 1.0f) tau = 1.0f;
        cos_conformal_report_t r;
        if (cos_conformal_eval_at(samples, n, tau, delta, &r) != 0) continue;

        double acc = (r.n_accepted > 0)
            ? (double)(r.n_accepted - r.n_errors_accepted) / (double)r.n_accepted
            : 0.0;

        char acc_buf[32], ucb_buf[32];
        fmt_float_or_null(acc_buf, sizeof(acc_buf), acc,         r.n_accepted);
        fmt_float_or_null(ucb_buf, sizeof(ucb_buf), r.risk_ucb,  r.n_accepted);

        fprintf(fp,
            "    {\"tau\":%.4f, \"coverage\":%.6f, \"n_accept\":%d, "
            "\"accuracy\":%s, \"risk_ucb\":%s}%s\n",
            tau, r.coverage, r.n_accepted, acc_buf, ucb_buf,
            (k + 1 < n_steps) ? "," : "");

        printf("  %-6.2f %-10.4f %-18s %-10s %-10d\n",
               tau, r.coverage, acc_buf, ucb_buf, r.n_accepted);
    }
    fprintf(fp, "  ]\n}\n");
    fclose(fp);

    printf("\n  wrote %s  (%d steps, %d scored rows, acc(all)=%.4f)\n",
           outpath, n_steps, n_scored, acc_all);

    free(samples);
    return 0;
}
