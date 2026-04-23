/*
 * σ-mission — long-horizon checkpointed execution with σ drift awareness.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MISSION_H
#define COS_SIGMA_MISSION_H

#include "error_attribution.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_MISSION_MAX_STEPS 64
#define COS_MISSION_ID_CHARS  36

enum {
    COS_MISSION_STEP_PENDING      = 0,
    COS_MISSION_STEP_RUNNING      = 1,
    COS_MISSION_STEP_DONE         = 2,
    COS_MISSION_STEP_FAILED       = 3,
    COS_MISSION_STEP_ROLLED_BACK  = 4,
};

struct cos_mission_step {
    int   step_id;
    char  description[512];
    char  result[4096];
    float sigma;
    enum cos_error_source attribution;
    int   status;
    int64_t started_ms;
    int64_t finished_ms;
    char  checkpoint_hash[65];
};

struct cos_mission {
    char  goal[1024];
    struct cos_mission_step steps[COS_MISSION_MAX_STEPS];
    int   n_steps;
    int   current_step;
    float sigma_mean;
    float sigma_trend;
    int   coherence_status;
    int   paused;
    int64_t started_ms;
    int64_t elapsed_ms;
    char  mission_id[COS_MISSION_ID_CHARS + 1];
    char  pause_reason[256];
};

int cos_mission_create(const char *goal, struct cos_mission *m);

int cos_mission_run(struct cos_mission *m);

int cos_mission_step_execute(struct cos_mission *m, int step_idx);

int cos_mission_checkpoint_save(struct cos_mission *m, int step_idx);

int cos_mission_checkpoint_restore(struct cos_mission *m, int step_idx);

int cos_mission_rollback(struct cos_mission *m);

int cos_mission_pause(struct cos_mission *m, const char *reason);

int cos_mission_resume(struct cos_mission *m);

/** Caller must free(NULL-safe) non-NULL return with free(). */
char *cos_mission_report(const struct cos_mission *m);

void cos_mission_print_status(const struct cos_mission *m);

float cos_mission_sigma_trend(const struct cos_mission *m);

int cos_mission_persist(const struct cos_mission *m, const char *path);

int cos_mission_load(struct cos_mission *m, const char *path);

/** For harness: unit tests without live LLM (synthetic σ and text). */
int cos_mission_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_MISSION_H */
