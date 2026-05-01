/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Discover — declarative hypothesis harness.  OMEGA-5.
 *
 * NOT "LLM autogenerates hypotheses".  That's bragging, not science.
 *
 * σ-Discover runs a pre-declared list of experiments, each with a
 * hypothesis string, a shell command to execute, and a pass/fail
 * criterion on that command's output.  Every result is logged
 * append-only to a JSONL file with exit status, elapsed time, and
 * verdict (confirmed / rejected / inconclusive).
 *
 * This is enough to automate:
 *   - "does evolved-weight σ outperform uniform σ on TruthfulQA 817?"
 *   - "does the engram hit rate drop when cache-cap is halved?"
 *   - "does σ_consistency alone beat σ_combined on HellaSwag?"
 *
 * The harness doesn't reason about hypotheses.  It runs them,
 * records, and moves on.  The scientist is still you.
 *
 * Input file format (JSONL, one experiment per line):
 *   {"id":"hx1","hypothesis":"…","cmd":"…","expect":"substr",
 *    "expect_kind":"contains|not_contains|exit_0|exit_nz"}
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_DISCOVER_H
#define COS_SIGMA_DISCOVER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_DISCOVER_EXIT_0       = 0,
    COS_DISCOVER_EXIT_NZ      = 1,
    COS_DISCOVER_CONTAINS     = 2,
    COS_DISCOVER_NOT_CONTAINS = 3,
} cos_discover_expect_kind_t;

typedef enum {
    COS_DISCOVER_CONFIRMED    = 0,
    COS_DISCOVER_REJECTED     = 1,
    COS_DISCOVER_INCONCLUSIVE = 2,   /* cmd failed to run at all */
} cos_discover_verdict_t;

typedef struct {
    char   id[64];
    char   hypothesis[256];
    char   cmd[1024];
    char   expect[256];
    cos_discover_expect_kind_t expect_kind;
} cos_discover_experiment_t;

typedef struct {
    char   id[64];
    int    exit_code;
    double elapsed_ms;
    cos_discover_verdict_t verdict;
    char   stdout_snippet[512];
} cos_discover_result_t;

/* Run a single experiment.  Blocks on popen(); captures up to 64 KiB
 * of combined stdout/stderr into an internal buffer, truncates a
 * snippet into result->stdout_snippet. */
int  cos_discover_run_one(const cos_discover_experiment_t *ex,
                          cos_discover_result_t *out);

/* Append one result to a JSONL log (one line).  Creates the file if
 * missing.  Returns 0 on success. */
int  cos_discover_log_append_jsonl(const char *path,
                                   const cos_discover_experiment_t *ex,
                                   const cos_discover_result_t *r);

int  cos_discover_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_DISCOVER_H */
