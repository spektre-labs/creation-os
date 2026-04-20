/*
 * σ-benchmark suite.
 *
 * Aggregates N benchmarks into a single table:
 *     name | accuracy | accuracy_accepted | coverage |
 *     sigma_mean | cost_eur | rethink_rate | escalation_rate
 *
 * A run produces a cos_bench_suite_report_t; the same struct is the
 * baseline stored on disk.  The regression gate compares a fresh
 * run against the baseline and flags any metric that moved the
 * wrong way by more than τ_regression.  If any metric regresses
 * the suite returns COS_BENCH_REGRESSION so CI blocks the merge.
 *
 * Metrics are normalised so "up is good":
 *   accuracy ↑, accuracy_accepted ↑, coverage ↑
 *   sigma_mean ↓, cost_eur ↓, rethink_rate ↓, escalation_rate ↓
 *
 * The built-in sampler is deterministic — it draws fake but stable
 * results from a seeded generator so CI gates can be frozen.  A
 * real pipeline plugs in a cos_bench_sample_fn callback that
 * executes the actual benchmark.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_BENCH_SUITE_H
#define COS_SIGMA_BENCH_SUITE_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_BENCH_MAX_ROWS  16
#define COS_BENCH_NAME_MAX  32

enum cos_bench_status {
    COS_BENCH_OK         =  0,
    COS_BENCH_ERR_ARG    = -1,
    COS_BENCH_ERR_IO     = -2,
    COS_BENCH_ERR_FORMAT = -3,
    COS_BENCH_REGRESSION = -4,
};

typedef struct {
    char  name[COS_BENCH_NAME_MAX];
    int   n_questions;
    float accuracy;
    float accuracy_accepted;
    float coverage;
    float sigma_mean;
    float cost_eur;
    float rethink_rate;
    float escalation_rate;
} cos_bench_result_t;

typedef struct {
    cos_bench_result_t rows[COS_BENCH_MAX_ROWS];
    int    n_rows;
    float  weighted_sigma;     /* weighted by n_questions           */
    float  mean_accuracy;
    float  total_cost_eur;
    int    regressions;        /* count after compare_baseline      */
} cos_bench_suite_report_t;

/* Benchmark recipe: name + question count (caller-supplied). */
typedef struct {
    char name[COS_BENCH_NAME_MAX];
    int  n_questions;
} cos_bench_job_t;

/* Caller hook.  Return the measured row by filling `out` and
 * returning 0 on success; non-zero aborts the suite. */
typedef int (*cos_bench_sample_fn)(const cos_bench_job_t *job,
                                   cos_bench_result_t *out,
                                   void *user);

/* ==================================================================
 * Running
 * ================================================================== */

/* Execute every job into `report`.  `sample` may be NULL → built-in
 * deterministic sampler. */
int cos_bench_suite_run(const cos_bench_job_t *jobs, int n_jobs,
                        cos_bench_sample_fn sample, void *user,
                        cos_bench_suite_report_t *report);

/* Built-in deterministic sampler.  Stable across hosts because it
 * is seeded from FNV-1a(name). */
int cos_bench_suite_sample_sim(const cos_bench_job_t *job,
                               cos_bench_result_t *out,
                               void *user);

/* ==================================================================
 * Persistence
 * ================================================================== */

/* Serialise a report to FILE* in a fixed line-based format so the
 * baseline diff is human-readable and git-friendly. */
int cos_bench_suite_write(FILE *fp, const cos_bench_suite_report_t *report);
int cos_bench_suite_read (FILE *fp, cos_bench_suite_report_t *report);

/* ==================================================================
 * Regression gate
 * ================================================================== */

typedef struct {
    char  name[COS_BENCH_NAME_MAX];
    char  metric[COS_BENCH_NAME_MAX];
    float baseline;
    float current;
    float delta;       /* current - baseline                       */
} cos_bench_regression_t;

/* Compare `current` against `baseline`; write up to cap
 * regressions into `out`; return 0 when nothing regressed by more
 * than `tau` for any metric, COS_BENCH_REGRESSION otherwise. */
int cos_bench_suite_compare(const cos_bench_suite_report_t *baseline,
                            const cos_bench_suite_report_t *current,
                            float tau,
                            cos_bench_regression_t *out, int cap,
                            int *n_out);

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_bench_suite_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_BENCH_SUITE_H */
