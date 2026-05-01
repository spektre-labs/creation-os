/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-pipeline: multi-dataset evaluation (SCI-6).
 *
 * A thin aggregator on top of SCI-1 (conformal τ) and SCI-3 (coverage
 * curve): given a list of detail JSONLs — one per benchmark dataset
 * — produce a single summary table
 *
 *     name | n_rows | n_scored | acc(all) | acc(accepted) |
 *          | coverage | σ_mean  | τ_conformal | valid
 *
 * where "accepted" refers to acceptance at the conformal τ computed
 * on that dataset at the requested (α, δ).  The numbers are pulled
 * directly from the JSONL rows ⇒ no simulation, no hidden state.
 *
 * For datasets whose detail JSONLs do not yet exist on this checkout
 * the aggregator records a "measured": false row (see
 * `cos_suite_sci_eval_missing`) so the operator can still render a
 * complete table without fabricating numbers.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SUITE_SCI_H
#define COS_SIGMA_SUITE_SCI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SUITE_SCI_MAX_ROWS  16
#define COS_SUITE_SCI_NAME_MAX  32
#define COS_SUITE_SCI_PATH_MAX  256

typedef struct {
    char  name[COS_SUITE_SCI_NAME_MAX];    /* "truthfulqa" etc.        */
    char  detail_path[COS_SUITE_SCI_PATH_MAX];
    int   measured;                        /* 1 if JSONL was loaded    */
    int   n_rows;
    int   n_scored;
    int   n_correct;
    float accuracy_all;     /* n_correct / n_scored               */
    float coverage_scored;  /* n_scored  / n_rows   (harness)     */
    float sigma_mean;
    /* conformal τ @ (α, δ) on this dataset -------------------------- */
    float alpha;
    float delta;
    float tau;
    int   tau_valid;
    int   n_accepted;
    int   n_errors_accepted;
    float accuracy_accepted;
    float coverage_accepted; /* n_accepted / n_scored             */
    float risk_ucb;
} cos_suite_sci_row_t;

typedef struct {
    int n_rows;
    cos_suite_sci_row_t rows[COS_SUITE_SCI_MAX_ROWS];
} cos_suite_sci_report_t;

/* Evaluate a single detail JSONL at (α, δ) under an optional
 * "mode" filter (e.g. "pipeline" for TruthfulQA).  `mode_filter`
 * may be NULL or "" to mean "no filter".
 *
 * On success returns 0 and sets out->measured = 1.  If the file does
 * not exist, returns 0 but sets out->measured = 0 so the aggregator
 * can render a complete table without crashing. */
int cos_suite_sci_eval(const char *name,
                       const char *detail_path,
                       const char *mode_filter,
                       float alpha, float delta,
                       cos_suite_sci_row_t *out);

/* Emit the report as pretty JSON to `path`.  Schema:
 *   {"schema":"cos.suite_sci.v1",
 *    "alpha":..., "delta":...,
 *    "rows":[ { ... cos_suite_sci_row_t ... }, ... ]} */
int cos_suite_sci_write_json(const char *path,
                             const cos_suite_sci_report_t *rep,
                             float alpha, float delta);

/* Self-test: exercises the eval on the shipped TruthfulQA detail
 * JSONL if present; falls back to a synthetic monotone dataset
 * otherwise.  Returns 0 on success. */
int cos_suite_sci_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_SUITE_SCI_H */
