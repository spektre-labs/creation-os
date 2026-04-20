/*
 * σ-multi-agent orchestration.
 *
 * One coordinator + N worker agents, each bound to a role
 * ("researcher" | "coder" | "reviewer" | "writer" | "coordinator").
 * The coordinator calls cos_team_plan() to decompose a goal into an
 * ordered sequence of steps, each tagged with the role that should
 * attempt it.  cos_team_run() walks the plan: for every step we
 * pick the highest-capability agent for the requested role, run it,
 * record σ, and if σ > τ_retry we try the next best candidate
 * (up to COS_TEAM_MAX_RETRIES).  Any step that still fails escalates
 * the entire turn so the caller can hand off upstream.
 *
 * All state lives in-memory; the kernel never allocates and returns
 * byte-stable JSON so CI can diff two runs bit-for-bit.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_TEAM_H
#define COS_SIGMA_TEAM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_TEAM_MAX_AGENTS    8
#define COS_TEAM_MAX_STEPS    16
#define COS_TEAM_ID_MAX       32
#define COS_TEAM_ROLE_MAX     24
#define COS_TEAM_TEXT_MAX    256
#define COS_TEAM_MAX_RETRIES  2

#define COS_TEAM_TAU_RETRY_DEFAULT 0.35f
#define COS_TEAM_TAU_ABORT_DEFAULT 0.60f

enum cos_team_status {
    COS_TEAM_OK       =  0,
    COS_TEAM_ERR_ARG  = -1,
    COS_TEAM_ERR_FULL = -2,
    COS_TEAM_FAIL     = -3,  /* at least one step escalated */
};

typedef struct {
    char  id[COS_TEAM_ID_MAX];
    char  role[COS_TEAM_ROLE_MAX];
    float capability;    /* 0..1; higher = more trusted for this role */
    int   uses;          /* how many steps it executed in last run    */
} cos_team_agent_t;

typedef struct {
    int   index;                          /* 0..n_steps-1            */
    char  description[COS_TEAM_TEXT_MAX]; /* human-readable          */
    char  required_role[COS_TEAM_ROLE_MAX];
    char  assigned[COS_TEAM_ID_MAX];      /* agent id on last try    */
    float sigma;                          /* σ of the accepted run   */
    int   attempts;                       /* # of agents that tried  */
    int   escalated;                      /* 1 = no candidate passed */
} cos_team_step_t;

typedef struct {
    char  goal[COS_TEAM_TEXT_MAX];
    cos_team_step_t steps[COS_TEAM_MAX_STEPS];
    int   n_steps;
    float sigma_goal;    /* mean σ across steps after the run       */
    int   escalations;   /* count of escalated steps                */
} cos_team_plan_t;

typedef struct {
    cos_team_agent_t agents[COS_TEAM_MAX_AGENTS];
    int              n_agents;
    char             coordinator_id[COS_TEAM_ID_MAX];
    float            tau_retry;
    float            tau_abort;
} cos_team_t;

/* ==================================================================
 * Lifecycle
 * ================================================================== */
void cos_team_init(cos_team_t *team,
                   const char *coordinator_id);

/* Add an agent; returns the registered slot on success. */
int  cos_team_add_agent(cos_team_t *team,
                        const char *id,
                        const char *role,
                        float capability);

/* Override retry / abort thresholds. */
void cos_team_set_thresholds(cos_team_t *team, float tau_retry, float tau_abort);

/* ==================================================================
 * Planning
 *
 * Decomposes a goal into a canonical 5-step plan
 * (research → design → implement → review → write-up).
 * Steps are stable given the same goal string.
 * ================================================================== */
int  cos_team_plan(cos_team_t *team,
                   const char *goal,
                   cos_team_plan_t *plan);

/* ==================================================================
 * Execution
 *
 * For each step: pick the top-capability agent in the requested
 * role, run the simulated executor, measure σ, retry the next best
 * agent when σ > tau_retry.  If no agent closes σ below tau_abort,
 * the step is flagged escalated (and the step σ is the last one
 * measured).  Returns COS_TEAM_OK when nothing escalated.
 * ================================================================== */

/* Simulated step executor: given (step, agent) return a σ in [0,1].
 * The reference kernel uses a deterministic pseudo-random schedule
 * driven by (goal_hash, step_index, agent_capability) so runs are
 * reproducible.  A plug-in backend can override this later. */
typedef float (*cos_team_exec_fn)(const cos_team_plan_t *plan,
                                  const cos_team_step_t *step,
                                  const cos_team_agent_t *agent,
                                  int attempt,
                                  void *user);

int  cos_team_run(cos_team_t *team,
                  cos_team_plan_t *plan,
                  cos_team_exec_fn exec,
                  void *user);

/* Built-in deterministic executor (used when exec == NULL). */
float cos_team_exec_sim(const cos_team_plan_t *plan,
                        const cos_team_step_t *step,
                        const cos_team_agent_t *agent,
                        int attempt,
                        void *user);

/* ==================================================================
 * Reporting & self-test
 * ================================================================== */
int  cos_team_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_TEAM_H */
