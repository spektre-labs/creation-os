/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-benchmark-suite demo.
 *
 * Runs a fixed five-benchmark job list against the built-in
 * deterministic sampler, writes a baseline file in /tmp, re-runs
 * the suite, compares current vs. baseline, and injects a synthetic
 * regression so the gate exercises both branches.  Output is a
 * stable JSON envelope that the CI script parses.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "bench_suite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static void print_row(const cos_bench_result_t *r, int last) {
    printf("    { \"name\": \"%s\", \"n_questions\": %d,"
           " \"accuracy\": %.4f, \"accuracy_accepted\": %.4f,"
           " \"coverage\": %.4f, \"sigma_mean\": %.4f,"
           " \"cost_eur\": %.6f, \"rethink_rate\": %.4f,"
           " \"escalation_rate\": %.4f }%s\n",
           r->name, r->n_questions,
           (double)r->accuracy, (double)r->accuracy_accepted,
           (double)r->coverage, (double)r->sigma_mean,
           (double)r->cost_eur, (double)r->rethink_rate,
           (double)r->escalation_rate, last ? "" : ",");
}

int main(void) {
    int st = cos_bench_suite_self_test();

    cos_bench_job_t jobs[] = {
        { "truthfulqa",    817 },
        { "arc_challenge", 1172 },
        { "arc_easy",      2376 },
        { "hellaswag",     746 },
        { "mmlu_top7",     700 },
    };
    int n = (int)(sizeof jobs / sizeof jobs[0]);

    cos_bench_suite_report_t base, cur;
    cos_bench_suite_run(jobs, n, NULL, NULL, &base);

    /* Persist & reload the baseline to exercise the on-disk format. */
    char path[] = "/tmp/cos-bench-baseline-XXXXXX";
    int fd = mkstemp(path);
    FILE *fp = fdopen(fd, "w+");
    cos_bench_suite_write(fp, &base);
    fclose(fp);

    cos_bench_suite_report_t base_loaded;
    fp = fopen(path, "r");
    cos_bench_suite_read(fp, &base_loaded);
    fclose(fp);
    remove(path);

    /* Fresh run: identical to baseline ⇒ zero regressions. */
    cos_bench_suite_run(jobs, n, NULL, NULL, &cur);
    cos_bench_regression_t regs_clean[16];
    int n_clean = 0;
    int rc_clean = cos_bench_suite_compare(&base_loaded, &cur, 0.02f,
                                           regs_clean, 16, &n_clean);

    /* Inject a synthetic regression on truthfulqa. */
    cos_bench_suite_report_t bad = cur;
    bad.rows[0].sigma_mean   += 0.10f;
    bad.rows[0].accuracy     -= 0.15f;
    bad.rows[0].escalation_rate += 0.05f;

    cos_bench_regression_t regs_bad[16];
    int n_bad = 0;
    int rc_bad = cos_bench_suite_compare(&base_loaded, &bad, 0.02f,
                                         regs_bad, 16, &n_bad);

    printf("{\n");
    printf("  \"kernel\": \"sigma_bench_suite\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"rows\": [\n");
    for (int i = 0; i < cur.n_rows; i++) {
        print_row(&cur.rows[i], i + 1 == cur.n_rows);
    }
    printf("  ],\n");
    printf("  \"weighted_sigma\": %.4f,\n", (double)cur.weighted_sigma);
    printf("  \"mean_accuracy\": %.4f,\n", (double)cur.mean_accuracy);
    printf("  \"total_cost_eur\": %.6f,\n", (double)cur.total_cost_eur);
    printf("  \"clean_run\": { \"rc\": %d, \"regressions\": %d },\n",
           rc_clean, n_clean);
    printf("  \"injected_regression\": { \"rc\": %d, \"regressions\": %d,\n",
           rc_bad, n_bad);
    printf("    \"details\": [\n");
    for (int i = 0; i < n_bad; i++) {
        printf("      { \"benchmark\": \"%s\", \"metric\": \"%s\","
               " \"baseline\": %.4f, \"current\": %.4f,"
               " \"delta\": %.4f }%s\n",
               regs_bad[i].name, regs_bad[i].metric,
               (double)regs_bad[i].baseline, (double)regs_bad[i].current,
               (double)regs_bad[i].delta,
               i + 1 == n_bad ? "" : ",");
    }
    printf("    ]\n  }\n");
    printf("}\n");

    return st == 0 ? 0 : 1;
}
