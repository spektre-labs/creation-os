/*
 * cos think — σ-orchestrated goal decomposition over the chat pipeline.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_CLI_THINK_H
#define COS_CLI_THINK_H

#include "error_attribution.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_THINK_MAX_SUBTASKS 16

struct cos_think_subtask {
    char prompt[1024];
    char answer[4096];
    float sigma_combined;
    enum cos_error_source attribution;
    int   backend; /* 0 LOCAL, 1 CLOUD */
    int64_t latency_ms;
    /** Terminal σ-gate action for this subtask run (COS_SIGMA_ACTION_*). */
    int   final_action;
};

struct cos_think_result {
    char    goal[1024];
    int     n_subtasks;
    struct cos_think_subtask subtasks[COS_THINK_MAX_SUBTASKS];
    float   sigma_mean;
    float   sigma_min;
    float   sigma_max;
    int     best_subtask_idx;
    int     consensus;
    int64_t total_ms;
    int64_t timestamp;
};

/** Run decomposition + one pipeline pass per subtask (local first). */
int cos_think_run(const char *goal, int max_rounds,
                  struct cos_think_result *result);

/** Ask the configured local generator to split `goal` into lines. */
int cos_think_decompose(const char *goal, char subtasks[][1024], int *n_out);

/** `malloc`'d UTF-8 JSON — caller `free()`s when non-NULL. */
char *cos_think_report_to_json(const struct cos_think_result *r);

/** Writes JSON next to ~/.cos/think/<timestamp>_<slug>.json */
int cos_think_persist(const struct cos_think_result *r);

void cos_think_print_report(const struct cos_think_result *r);

/** argv are flags only (--goal …), not `cos think`. Returns exit status. */
int cos_think_main(int argc, char **argv);

int cos_think_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_CLI_THINK_H */
