/*
 * Long-horizon planning with σ-checkpoints (HORIZON-3).
 *
 * Decomposes a mission into ordered steps; after each step (simulated
 * or real) a σ measurement is combined with the standard three-state
 * reinforce gate.  Snapshots are written to disk under a run root so a
 * failed step can roll back to the last good checkpoint.
 *
 * v0: step σ values are supplied by the caller (or a built-in mission
 * template).  There is no LLM planner — the architectural hook is the
 * checkpoint + rollback discipline, not autonomous task decomposition.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_LONG_HORIZON_H
#define COS_LONG_HORIZON_H

#include "../pipeline/reinforce.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_LH_MAX_STEPS 32

typedef struct {
    char  description[256];
    float sigma;   /* σ after the step's "execution" (caller-measured) */
} cos_lh_step_spec_t;

typedef struct {
    int                step_id;       /* 1-based for display           */
    char               description[256];
    float              sigma;
    cos_sigma_action_t action;
    int                rethink_count;
    char               snapshot_path[512];
} cos_lh_step_result_t;

typedef struct {
    int     n_spec;           /* total steps planned                    */
    int     n_executed;       /* steps actually finished (ACCEPT)      */
    int     aborted;          /* 1 if stopped on ABSTAIN / τ_abort      */
    int     rollback_snapshot;/* last good 1-based snapshot id (0=none) */
    float   sigma_plan_max;   /* max σ over visited steps (incl. fail) */
    float   dsigma_dt;        /* (σ_last−σ_first)/max(1,n_visit−1)       */
    cos_lh_step_result_t step[COS_LH_MAX_STEPS];
} cos_lh_report_t;

/* Default τ_abort above which we force abort regardless of ACCEPT band. */
#define COS_LH_DEFAULT_TAU_ABORT 0.72f

/* Run checkpointed plan.  snapshot_root is created; each successful
 * step writes snapshot_root/snap_<k>/MANIFEST.txt.  Returns 0 on full
 * completion, 1 if aborted (rollback — still a "successful" harness
 * outcome), -1 on argument error. */
int cos_long_horizon_run(const cos_lh_step_spec_t *specs, int n_spec,
                         float tau_accept, float tau_rethink, float tau_abort,
                         const char *snapshot_root,
                         cos_lh_report_t *rep);

/* Built-in demo mission (English keywords: organize + downloads). */
int cos_long_horizon_mission_downloads(cos_lh_step_spec_t *out, int *n_out,
                                       int fail_at_step3);

/* Match mission text to a template; returns -1 if unknown. */
int cos_long_horizon_mission_from_text(const char *mission,
                                       cos_lh_step_spec_t *out, int *n_out,
                                       int fail_step3);

/* Deterministic regression.  Returns 0 on success. */
int cos_long_horizon_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_LONG_HORIZON_H */
