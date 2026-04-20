/*
 * σ-benchmark suite — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "bench_suite.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------ helpers ------------------------ */

static void sncpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static uint64_t bs_fnv1a(const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 0x100000001B3ULL; }
    return h;
}

static uint64_t bs_lcg(uint64_t *s) {
    *s = (*s) * 6364136223846793005ULL + 1442695040888963407ULL;
    return *s;
}

static float bs_unit(uint64_t *s) {
    return (float)((double)(bs_lcg(s) >> 11) / (double)((uint64_t)1 << 53));
}

static float clip01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

/* ------------------------ sampler ------------------------ */

int cos_bench_suite_sample_sim(const cos_bench_job_t *job,
                               cos_bench_result_t *out,
                               void *user) {
    (void)user;
    if (!job || !out) return COS_BENCH_ERR_ARG;
    memset(out, 0, sizeof *out);
    sncpy(out->name, sizeof out->name, job->name);
    out->n_questions = job->n_questions;

    uint64_t s = bs_fnv1a(job->name, strlen(job->name));
    if (s == 0) s = 0xDEADBEEFULL;
    /* Draw values in plausible ranges. */
    out->accuracy          = 0.60f + 0.30f * bs_unit(&s);   /* 0.60..0.90 */
    out->accuracy_accepted = clip01(out->accuracy + 0.05f + 0.05f * bs_unit(&s));
    out->coverage          = 0.70f + 0.25f * bs_unit(&s);
    out->sigma_mean        = 0.10f + 0.15f * bs_unit(&s);   /* 0.10..0.25 */
    out->cost_eur          = (float)job->n_questions * (0.00008f + 0.00012f * bs_unit(&s));
    out->rethink_rate      = 0.05f + 0.10f * bs_unit(&s);
    out->escalation_rate   = 0.03f + 0.07f * bs_unit(&s);
    return COS_BENCH_OK;
}

/* ------------------------ run ------------------------ */

int cos_bench_suite_run(const cos_bench_job_t *jobs, int n_jobs,
                        cos_bench_sample_fn sample, void *user,
                        cos_bench_suite_report_t *report) {
    if (!jobs || n_jobs <= 0 || !report) return COS_BENCH_ERR_ARG;
    if (n_jobs > COS_BENCH_MAX_ROWS) return COS_BENCH_ERR_ARG;
    if (!sample) sample = cos_bench_suite_sample_sim;

    memset(report, 0, sizeof *report);
    double acc_sum = 0.0, cost_sum = 0.0;
    double sigma_weighted = 0.0;
    long   total_q = 0;

    for (int i = 0; i < n_jobs; i++) {
        cos_bench_result_t *r = &report->rows[i];
        int rc = sample(&jobs[i], r, user);
        if (rc != 0) return rc;
        acc_sum  += r->accuracy;
        cost_sum += r->cost_eur;
        sigma_weighted += (double)r->sigma_mean * (double)r->n_questions;
        total_q  += r->n_questions;
    }
    report->n_rows        = n_jobs;
    report->mean_accuracy = (float)(acc_sum / (double)n_jobs);
    report->total_cost_eur = (float)cost_sum;
    report->weighted_sigma = total_q > 0
        ? (float)(sigma_weighted / (double)total_q)
        : 0.0f;
    report->regressions = 0;
    return COS_BENCH_OK;
}

/* ------------------------ persistence ------------------------
 *
 * Fixed line format:
 *     COS_BENCH_V1
 *     ROWS <n>
 *     <name> <n_q> <acc> <acc_acc> <cov> <sigma> <cost> <rethink> <escal>
 *     ... (n times)
 */

int cos_bench_suite_write(FILE *fp, const cos_bench_suite_report_t *report) {
    if (!fp || !report) return COS_BENCH_ERR_ARG;
    if (fprintf(fp, "COS_BENCH_V1\nROWS %d\n", report->n_rows) < 0)
        return COS_BENCH_ERR_IO;
    for (int i = 0; i < report->n_rows; i++) {
        const cos_bench_result_t *r = &report->rows[i];
        if (fprintf(fp, "%s %d %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n",
                    r->name, r->n_questions,
                    (double)r->accuracy, (double)r->accuracy_accepted,
                    (double)r->coverage, (double)r->sigma_mean,
                    (double)r->cost_eur, (double)r->rethink_rate,
                    (double)r->escalation_rate) < 0)
            return COS_BENCH_ERR_IO;
    }
    return COS_BENCH_OK;
}

int cos_bench_suite_read(FILE *fp, cos_bench_suite_report_t *report) {
    if (!fp || !report) return COS_BENCH_ERR_ARG;
    memset(report, 0, sizeof *report);
    char tag[32] = {0};
    if (fscanf(fp, "%31s", tag) != 1) return COS_BENCH_ERR_FORMAT;
    if (strcmp(tag, "COS_BENCH_V1") != 0) return COS_BENCH_ERR_FORMAT;
    int n;
    if (fscanf(fp, "%31s %d", tag, &n) != 2) return COS_BENCH_ERR_FORMAT;
    if (strcmp(tag, "ROWS") != 0) return COS_BENCH_ERR_FORMAT;
    if (n <= 0 || n > COS_BENCH_MAX_ROWS) return COS_BENCH_ERR_FORMAT;
    report->n_rows = n;

    double acc_sum = 0.0, cost_sum = 0.0;
    double sigma_weighted = 0.0;
    long   total_q = 0;

    for (int i = 0; i < n; i++) {
        cos_bench_result_t *r = &report->rows[i];
        float acc, aacc, cov, sig, cost, reth, esc;
        int   q;
        if (fscanf(fp, "%31s %d %f %f %f %f %f %f %f",
                   r->name, &q, &acc, &aacc, &cov, &sig, &cost, &reth, &esc) != 9)
            return COS_BENCH_ERR_FORMAT;
        r->n_questions       = q;
        r->accuracy          = acc;
        r->accuracy_accepted = aacc;
        r->coverage          = cov;
        r->sigma_mean        = sig;
        r->cost_eur          = cost;
        r->rethink_rate      = reth;
        r->escalation_rate   = esc;
        acc_sum += acc; cost_sum += cost;
        sigma_weighted += (double)sig * (double)q;
        total_q += q;
    }
    report->mean_accuracy = (float)(acc_sum / (double)n);
    report->total_cost_eur = (float)cost_sum;
    report->weighted_sigma = total_q > 0
        ? (float)(sigma_weighted / (double)total_q)
        : 0.0f;
    return COS_BENCH_OK;
}

/* ------------------------ regression ------------------------ */

static int push_regression(cos_bench_regression_t *out, int cap, int *n,
                           const char *name, const char *metric,
                           float baseline, float current) {
    if (!out || !n) return 0;
    if (*n >= cap) { (*n)++; return 1; }  /* still counted, not recorded */
    cos_bench_regression_t *r = &out[*n];
    sncpy(r->name,   sizeof r->name,   name);
    sncpy(r->metric, sizeof r->metric, metric);
    r->baseline = baseline;
    r->current  = current;
    r->delta    = current - baseline;
    (*n)++;
    return 1;
}

int cos_bench_suite_compare(const cos_bench_suite_report_t *baseline,
                            const cos_bench_suite_report_t *current,
                            float tau,
                            cos_bench_regression_t *out, int cap,
                            int *n_out) {
    if (!baseline || !current) return COS_BENCH_ERR_ARG;
    int n = 0;
    if (n_out) *n_out = 0;
    int rows = baseline->n_rows;
    if (current->n_rows < rows) rows = current->n_rows;

    for (int i = 0; i < rows; i++) {
        const cos_bench_result_t *b = &baseline->rows[i];
        const cos_bench_result_t *c = &current->rows[i];
        /* Match by name so reorderings don't create phantom regressions. */
        const cos_bench_result_t *cc = NULL;
        for (int j = 0; j < current->n_rows; j++) {
            if (strcmp(current->rows[j].name, b->name) == 0) {
                cc = &current->rows[j]; break;
            }
        }
        if (!cc) cc = c;
        /* up-is-good metrics: regression = fell by > tau */
        if ((b->accuracy          - cc->accuracy)          >  tau)
            push_regression(out, cap, &n, b->name, "accuracy",
                            b->accuracy, cc->accuracy);
        if ((b->accuracy_accepted - cc->accuracy_accepted) >  tau)
            push_regression(out, cap, &n, b->name, "accuracy_accepted",
                            b->accuracy_accepted, cc->accuracy_accepted);
        if ((b->coverage          - cc->coverage)          >  tau)
            push_regression(out, cap, &n, b->name, "coverage",
                            b->coverage, cc->coverage);
        /* down-is-good metrics: regression = rose by > tau */
        if ((cc->sigma_mean       - b->sigma_mean)         >  tau)
            push_regression(out, cap, &n, b->name, "sigma_mean",
                            b->sigma_mean, cc->sigma_mean);
        if ((cc->rethink_rate     - b->rethink_rate)       >  tau)
            push_regression(out, cap, &n, b->name, "rethink_rate",
                            b->rethink_rate, cc->rethink_rate);
        if ((cc->escalation_rate  - b->escalation_rate)    >  tau)
            push_regression(out, cap, &n, b->name, "escalation_rate",
                            b->escalation_rate, cc->escalation_rate);
    }
    if (n_out) *n_out = n;
    return n > 0 ? COS_BENCH_REGRESSION : COS_BENCH_OK;
}

/* ------------------------ self-test ------------------------ */

int cos_bench_suite_self_test(void) {
    cos_bench_job_t jobs[] = {
        { "truthfulqa",    817 },
        { "arc_challenge", 1172 },
        { "arc_easy",      2376 },
        { "hellaswag",     746 },
        { "mmlu_top7",     700 },
    };
    int n = (int)(sizeof jobs / sizeof jobs[0]);

    cos_bench_suite_report_t a, b;
    if (cos_bench_suite_run(jobs, n, NULL, NULL, &a) != COS_BENCH_OK) return -1;
    if (cos_bench_suite_run(jobs, n, NULL, NULL, &b) != COS_BENCH_OK) return -2;
    /* Determinism: identical inputs ⇒ identical rows. */
    for (int i = 0; i < n; i++) {
        if (a.rows[i].accuracy        != b.rows[i].accuracy)        return -3;
        if (a.rows[i].sigma_mean      != b.rows[i].sigma_mean)      return -4;
        if (a.rows[i].escalation_rate != b.rows[i].escalation_rate) return -5;
    }

    /* Round-trip through the persistence format. */
    char path[] = "/tmp/cos-bench-selftest-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return -6;
    FILE *fp = fdopen(fd, "w+");
    if (!fp) { close(fd); return -7; }
    if (cos_bench_suite_write(fp, &a) != COS_BENCH_OK) { fclose(fp); remove(path); return -8; }
    rewind(fp);
    cos_bench_suite_report_t c;
    int rc = cos_bench_suite_read(fp, &c);
    fclose(fp);
    remove(path);
    if (rc != COS_BENCH_OK) return -9;
    if (c.n_rows != n) return -10;
    for (int i = 0; i < n; i++) {
        if (strcmp(c.rows[i].name, a.rows[i].name) != 0) return -11;
        /* 6-digit round-trip precision is enough. */
        if (fabsf(c.rows[i].accuracy - a.rows[i].accuracy) > 1e-5f) return -12;
    }

    /* Regression detection: mutate `c` so truthfulqa sigma rises. */
    c.rows[0].sigma_mean   = a.rows[0].sigma_mean + 0.10f;
    c.rows[0].accuracy     = a.rows[0].accuracy   - 0.15f;
    cos_bench_regression_t regs[8];
    int n_regs = 0;
    int rc2 = cos_bench_suite_compare(&a, &c, 0.02f, regs, 8, &n_regs);
    if (rc2 != COS_BENCH_REGRESSION) return -13;
    if (n_regs < 2) return -14;

    /* No regression when baseline == current. */
    int n_regs2 = 0;
    if (cos_bench_suite_compare(&a, &a, 0.02f, regs, 8, &n_regs2) != COS_BENCH_OK) return -15;
    if (n_regs2 != 0) return -16;
    return 0;
}
