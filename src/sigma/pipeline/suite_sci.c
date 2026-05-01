/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-pipeline: multi-dataset evaluation (SCI-6). */
#include "suite_sci.h"
#include "conformal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_SAMPLES 8192

static int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode)) ? 1 : 0;
}

int cos_suite_sci_eval(const char *name,
                       const char *detail_path,
                       const char *mode_filter,
                       float alpha, float delta,
                       cos_suite_sci_row_t *out) {
    if (!name || !detail_path || !out) return -1;
    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "%s", name);
    snprintf(out->detail_path, sizeof(out->detail_path), "%s", detail_path);
    out->alpha = alpha;
    out->delta = delta;

    if (!file_exists(detail_path)) {
        out->measured = 0;
        return 0;
    }

    cos_conformal_sample_t *samples = (cos_conformal_sample_t *)
        calloc(MAX_SAMPLES, sizeof(*samples));
    if (!samples) return -1;

    int n = cos_conformal_load_jsonl(detail_path, mode_filter,
                                     samples, MAX_SAMPLES);
    if (n < 0) { free(samples); out->measured = 0; return 0; }
    out->measured = 1;
    out->n_rows   = n;

    int n_scored = 0, n_correct = 0;
    double sigma_sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sigma_sum += (double)samples[i].sigma; /* includes unscored rows */
        if (samples[i].correct < 0) continue;
        n_scored  += 1;
        n_correct += (samples[i].correct > 0) ? 1 : 0;
    }
    out->n_scored        = n_scored;
    out->n_correct       = n_correct;
    out->sigma_mean      = (n > 0) ? (float)(sigma_sum / (double)n) : 0.0f;
    out->accuracy_all    = (n_scored > 0)
        ? (float)n_correct / (float)n_scored : 0.0f;
    out->coverage_scored = (n > 0)
        ? (float)n_scored / (float)n : 0.0f;

    /* Conformal τ on this dataset. */
    cos_conformal_report_t r;
    if (cos_conformal_calibrate(samples, n, alpha, delta, &r) == 0) {
        out->tau                = r.tau;
        out->tau_valid          = r.valid;
        out->n_accepted         = r.n_accepted;
        out->n_errors_accepted  = r.n_errors_accepted;
        out->accuracy_accepted  = (r.n_accepted > 0)
            ? (float)(r.n_accepted - r.n_errors_accepted) /
              (float)r.n_accepted
            : 0.0f;
        out->coverage_accepted  = r.coverage;
        out->risk_ucb           = r.risk_ucb;
    }

    free(samples);
    return 0;
}

int cos_suite_sci_write_json(const char *path,
                             const cos_suite_sci_report_t *rep,
                             float alpha, float delta) {
    if (!path || !rep) return -1;

    /* Create parent dir if needed. */
    char dir[COS_SUITE_SCI_PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (*dir) (void)mkdir(dir, 0700);
    }

    FILE *fp = fopen(path, "w");
    if (!fp) return -1;

    fprintf(fp,
        "{\n"
        "  \"schema\": \"cos.suite_sci.v1\",\n"
        "  \"alpha\": %.6f,\n"
        "  \"delta\": %.6f,\n"
        "  \"rows\": [\n", alpha, delta);

    for (int i = 0; i < rep->n_rows; ++i) {
        const cos_suite_sci_row_t *r = &rep->rows[i];
        fprintf(fp,
            "    {\n"
            "      \"name\": \"%s\",\n"
            "      \"detail_path\": \"%s\",\n"
            "      \"measured\": %d,\n"
            "      \"n_rows\": %d,\n"
            "      \"n_scored\": %d,\n"
            "      \"n_correct\": %d,\n"
            "      \"accuracy_all\": %.6f,\n"
            "      \"coverage_scored\": %.6f,\n"
            "      \"sigma_mean\": %.6f,\n"
            "      \"alpha\": %.6f,\n"
            "      \"delta\": %.6f,\n"
            "      \"tau\": %.6f,\n"
            "      \"tau_valid\": %d,\n"
            "      \"n_accepted\": %d,\n"
            "      \"n_errors_accepted\": %d,\n"
            "      \"accuracy_accepted\": %.6f,\n"
            "      \"coverage_accepted\": %.6f,\n"
            "      \"risk_ucb\": %.6f\n"
            "    }%s\n",
            r->name, r->detail_path, r->measured, r->n_rows,
            r->n_scored, r->n_correct, r->accuracy_all,
            r->coverage_scored, r->sigma_mean, r->alpha, r->delta,
            r->tau, r->tau_valid, r->n_accepted,
            r->n_errors_accepted, r->accuracy_accepted,
            r->coverage_accepted, r->risk_ucb,
            (i + 1 < rep->n_rows) ? "," : "");
    }

    fprintf(fp, "  ]\n}\n");
    fclose(fp);
    return 0;
}

int cos_suite_sci_self_test(void) {
    cos_suite_sci_row_t row;
    memset(&row, 0, sizeof(row));

    /* Prefer the real TruthfulQA JSONL if the check is run from the
     * repo root; otherwise substitute a synthetic file we build on
     * the fly. */
    const char *real = "benchmarks/pipeline/truthfulqa_817_detail.jsonl";
    if (file_exists(real)) {
        if (cos_suite_sci_eval("truthfulqa", real, "pipeline",
                               0.30f, 0.10f, &row) != 0) return -1;
        if (!row.measured) return -1;
        if (row.n_rows  <= 0) return -1;
        if (row.n_scored <= 0) return -1;
        if (!(row.accuracy_all >= 0.0f && row.accuracy_all <= 1.0f)) return -1;
        return 0;
    }

    /* Synthetic fallback: build a tiny JSONL in /tmp.  We use
     * mkstemp + manual rename to sidestep mkstemps (non-POSIX).    */
    char tmp[] = "/tmp/cos_suite_sci_self_test_XXXXXX";
    int fd = mkstemp(tmp);
    if (fd < 0) return -1;
    char tmp2[64];
    snprintf(tmp2, sizeof(tmp2), "%s.jsonl", tmp);
    (void)close(fd);
    (void)remove(tmp);
    FILE *fp = fopen(tmp2, "w");
    if (!fp) return -1;
    for (int i = 0; i < 50; ++i) {
        float s = (float)i / 50.0f;
        const char *c = (s < 0.5f) ? "true" : "false";
        fprintf(fp,
            "{\"id\":\"s-%04d\",\"sigma\":%.4f,\"correct\":%s,"
            "\"mode\":\"pipeline\"}\n", i, s, c);
    }
    fclose(fp);

    int rc = cos_suite_sci_eval("synthetic", tmp2, NULL,
                                0.30f, 0.10f, &row);
    (void)remove(tmp2);
    if (rc != 0 || !row.measured) return -1;
    if (row.n_scored != 50) return -1;
    if (!(row.accuracy_all > 0.4f && row.accuracy_all < 0.6f)) return -1;
    return 0;
}
