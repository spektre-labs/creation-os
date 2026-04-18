/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v121 σ-Planning — HTN decomposition + MCTS backtracking with
 * σ-governance at every transition.
 *
 * v111.2 /v1/reason is a *single-turn* reasoning loop.  v121 lifts
 * that loop into a multi-step agent:
 *
 *   goal  →  decompose into subtasks
 *         →  route each subtask to a tool (sandbox / chat / swarm)
 *         →  execute, measure σ for each transition
 *         →  if σ > τ anywhere, MCTS-backtrack and pick another
 *            branch (bounded: `max_replans`)
 *
 * The planner itself doesn't own the tools.  It owns the *control
 * flow* and the σ contract: every node and every edge has a σ
 * observation, and the outer loop is obligated to re-plan when any
 * σ exceeds τ.  The tool stubs used in v121.0 self-tests return
 * fixed σ values so the merge-gate can verify the control flow
 * without wiring real backends.  v121.1 plugs in v112 function
 * calling, v113 sandbox, and v114 swarm.
 *
 * Three σ-channels are tracked per step:
 *   σ_decompose : how confident is the planner the subtask was the
 *                 right piece of the goal?
 *   σ_tool      : how confident is the planner the chosen tool is
 *                 right for this subtask?
 *   σ_result    : how confident is the executor the produced answer
 *                 actually satisfies the subtask?
 *
 * σ_step = geometric mean of the three channels, matching the v6/v7
 * σ-product convention (Gmean of confidence channels; keeps the
 * aggregate in [0,1] so τ thresholds are comparable to per-channel
 * values).
 */
#ifndef COS_V121_PLANNING_H
#define COS_V121_PLANNING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V121_MAX_STEPS      32
#define COS_V121_MAX_BRANCHES    4      /* per step, for MCTS backtracking */
#define COS_V121_MAX_REPLANS    16      /* safety bound */
#define COS_V121_MAX_STR       256

typedef enum {
    COS_V121_TOOL_CHAT    = 0,
    COS_V121_TOOL_SANDBOX = 1,
    COS_V121_TOOL_SWARM   = 2,
    COS_V121_TOOL_MEMORY  = 3,
} cos_v121_tool_t;

typedef struct cos_v121_step {
    int    step_index;               /* 0-based, in executed order */
    char   action  [COS_V121_MAX_STR];
    cos_v121_tool_t tool;
    int    branch_taken;             /* which branch we picked from the candidates */
    int    branches_available;
    float  sigma_decompose;
    float  sigma_tool;
    float  sigma_result;
    float  sigma_step;               /* product */
    int    replanned;                /* 1 if we arrived here after a backtrack */
    char   note    [COS_V121_MAX_STR];
} cos_v121_step_t;

typedef struct cos_v121_plan {
    char            goal  [COS_V121_MAX_STR];
    cos_v121_step_t steps [COS_V121_MAX_STEPS];
    int             n_steps;
    int             n_replans;       /* total MCTS-backtracks consumed */
    float           total_sigma;     /* mean σ_step over executed steps */
    float           max_sigma;       /* worst σ_step (to surface risk)   */
    int             aborted;         /* 1 if we exceeded max_replans     */
} cos_v121_plan_t;

typedef struct cos_v121_config {
    float tau_step;       /* if σ_step > τ_step → replan (default 0.60) */
    int   max_replans;    /* default 8 */
    int   max_steps;      /* default COS_V121_MAX_STEPS */
} cos_v121_config_t;

void cos_v121_config_defaults(cos_v121_config_t *cfg);

/* Candidate a step might take during planning.  The stub tool source
 * used by v121.0 self-tests returns a fixed set of these per step;
 * the planner picks the lowest-σ candidate, backtracks to the
 * next-lowest when the chosen one fails σ on execution. */
typedef struct cos_v121_candidate {
    char            action[COS_V121_MAX_STR];
    cos_v121_tool_t tool;
    float           sigma_decompose;
    float           sigma_tool;
    /* `sigma_result_on_execute` is revealed only after the branch is
     * chosen — the planner does NOT see it at selection time. */
    float           sigma_result_on_execute;
} cos_v121_candidate_t;

/* A step source is the contract the planner calls once per step.
 * Given the goal and the current step index, it must fill up to
 * `cap` candidates and return the count written.  A count of zero
 * means the plan is complete.  Determinism is required so tests are
 * reproducible. */
typedef int (*cos_v121_step_source_fn)(
    const char *goal, int step_index,
    cos_v121_candidate_t *out, int cap, void *user);

/* Run the planner.  `source` supplies candidates per step, `cfg` may
 * be NULL for defaults.  Returns 0 on success (plan executed to
 * completion without exceeding max_replans) or -1 on abort. */
int  cos_v121_run(const cos_v121_config_t *cfg,
                  const char *goal,
                  cos_v121_step_source_fn source,
                  void *source_user,
                  cos_v121_plan_t *out);

/* Serialise the plan as /v1/plan -shaped JSON:
 *
 *   {"goal":"...","plan":[{step,action,tool,σ_decompose,σ_tool,
 *                          σ_result,σ_step,replanned}],
 *    "total_σ":X,"max_σ":Y,"replans":N,"aborted":false}
 *
 * `tool` is rendered as a lowercase short name.  Returns bytes
 * written (excluding NUL) or a negative value if the buffer is too
 * small. */
int  cos_v121_plan_to_json(const cos_v121_plan_t *plan,
                           char *out, size_t cap);

/* Pure-C self-test.  Uses a deterministic synthetic step source to
 * verify:
 *   - decompose into 3 subtasks
 *   - lowest-σ branch selection
 *   - MCTS backtrack when σ_result > τ_step on chosen branch
 *   - replans counted, max_sigma reflects the worst step
 *   - JSON shape
 *   - abort path when branches exhaust
 * Returns 0 on pass. */
int  cos_v121_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
