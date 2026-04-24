/*
 * cos life — personal daemon: tasks, learn, reports (orchestration only).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_CLI_LIFE_H
#define COS_CLI_LIFE_H

#include "../sigma/autonomy.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    COS_LIFE_MODE_SLEEPING = 0,
    COS_LIFE_MODE_WORKING  = 1,
    COS_LIFE_MODE_LEARNING = 2,
    COS_LIFE_MODE_WAITING  = 3,
};

struct cos_life_task {
    char    description[512];
    int     priority; /* 1 urgent 2 important 3 nice */
    int     status;   /* 0 TODO 1 IN_PROGRESS 2 DONE 3 BLOCKED */
    float   sigma;
    int64_t due_ms;
    int64_t created_ms;
    char    result[2048];
};

struct cos_life_state {
    struct cos_life_task     tasks[64];
    int                      n_tasks;
    struct cos_autonomy_state autonomy;
    int                      mode;
    char                     daily_summary[4096];
    float                    productivity_score;
    int64_t                  uptime_ms;
    int64_t                  started_ms;
};

int cos_life_init(struct cos_life_state *state);

int cos_life_add_task(const char *description, int priority, int64_t due_ms);

int cos_life_run_cycle(struct cos_life_state *state);

int cos_life_daily_summary(char *summary, int max_len);

int cos_life_report_overnight(char *report, int max_len);

int cos_life_main(int argc, char **argv);

#ifdef __cplusplus
}
#endif
#endif /* COS_CLI_LIFE_H */
