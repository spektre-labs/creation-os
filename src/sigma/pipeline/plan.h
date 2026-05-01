/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Plan — σ-gated multi-step planning with a hard budget.
 *
 * A tool call on its own is a reflex.  σ-Plan gives the agent a
 * sequence — "list files, filter .py, wc -l, summarize" — executes
 * the sequence step-by-step, and uses σ to decide when to STOP,
 * REPLAN, or let execution continue.
 *
 * The budget is the non-negotiable guard-rail: a plan *cannot*
 * exceed `max_steps`, `max_cost_eur`, or a per-step `max_risk`.
 * A frontier agent that can't respect a budget is a frontier agent
 * that mortgages the cloud bill at 3 a.m.; σ-Plan refuses.
 *
 * Per-step σ accounting:
 *
 *   σ_pre   — confidence this step is the right next move
 *             (selector output from σ-Tool)
 *   σ_post  — confidence the step's result was usable
 *             (σ_result from the executor)
 *   Δσ      = σ_post − σ_pre   ("was I more lost after this?")
 *
 * Plan-level σ:
 *
 *   σ_plan = mean(σ_pre over steps) + penalty(steps over budget)
 *
 * Replan trigger: the first step whose σ_post rises above a
 * caller-supplied `tau_replan` causes execute() to stop and return
 * COS_PLAN_REPLAN.  The caller generates a new plan from that step
 * onwards and resumes.
 *
 * Contracts (v0):
 *   1. init rejects NULL storage or non-positive capacity.
 *   2. add_step enforces the budget rails (risk, steps, cost).
 *   3. execute walks in order; stops on REPLAN or ABORT.
 *   4. Budget is checked both up-front (at add_step) AND at
 *      execute time (the executor's realized cost might differ
 *      from the tool's static `cost_eur`).
 *   5. Self-test covers happy-path plan, budget rejection,
 *      replan trigger, and abort on executor failure.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PLAN_H
#define COS_SIGMA_PLAN_H

#include <stddef.h>
#include <stdint.h>

#include "tool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_PLAN_OK        = 0,   /* plan finished */
    COS_PLAN_REPLAN    = 1,   /* σ_post > tau_replan; caller replans */
    COS_PLAN_ABORTED   = 2,   /* executor failure; caller decides */
    COS_PLAN_OVERBUDGET = 3,  /* runtime cost exceeded budget       */
    COS_PLAN_BLOCKED   = 4,   /* gate refused a step; stopped       */
} cos_plan_status_t;

typedef struct {
    const char     *description;
    cos_tool_call_t call;
    float           sigma_pre;      /* recorded at add_step time */
    float           sigma_post;     /* recorded at execute time  */
    int             completed;
    double          cost_eur;       /* realized                   */
} cos_plan_step_t;

typedef struct {
    int      max_steps;
    double   max_cost_eur;
    cos_tool_risk_t max_risk;
    float    tau_replan;       /* σ_post > this → REPLAN */
} cos_plan_budget_t;

typedef struct {
    const char       *goal;
    cos_plan_step_t  *steps;
    int               cap;
    int               count;
    cos_plan_budget_t budget;
    /* Running totals. */
    double            spent_eur;
    float             sigma_plan;   /* plan-level σ           */
    cos_plan_status_t status;
    int               replan_at;    /* index where σ_post spiked */
} cos_plan_t;

/* Initialise; caller owns `storage`. */
int  cos_sigma_plan_init(cos_plan_t *plan,
                         cos_plan_step_t *storage, int cap,
                         const char *goal,
                         cos_plan_budget_t budget);

/* Append a step.  Returns 0 on success, <0 if the budget rejects it.
 * -1 full, -2 risk too high, -3 would exceed cost,
 * -4 NULL/invalid tool call, -5 σ_pre out of [0,1]. */
int  cos_sigma_plan_add_step(cos_plan_t *plan,
                             const char *description,
                             cos_tool_call_t call,
                             float sigma_pre);

/* Execute the plan in order using `executor`, gating every step via
 * the agent's σ-Tool rule.  Returns the plan status; populates
 * `out_texts` (array of step.count pointers, caller-owned) with
 * executor output pointers. */
cos_plan_status_t
     cos_sigma_plan_execute(cos_plan_t *plan,
                            const cos_sigma_agent_t *agent,
                            cos_tool_executor_fn executor,
                            void *executor_ctx,
                            int confirm_all,
                            const char **out_texts);

/* Recompute plan-level σ from the current step table. */
float cos_sigma_plan_recompute_sigma(cos_plan_t *plan);

int   cos_sigma_plan_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_PLAN_H */
