/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v51 agent — σ-gated MCP-style agent loop (scaffold).
 *
 * Intent (honest): demonstrate the *shape* of an agent that will:
 *   - plan next action via the v51 cognitive loop
 *   - abstain and stop cleanly when σ is too high (no "try anyway" loop)
 *   - pass each tool invocation through a sandbox decision
 *   - append each (call, result) pair to a bounded experience buffer
 *
 * This file does NOT embed the v36 MCP server, the v48 sandbox, or any
 * real tool runtime — it defines plain-C seams so callers can plug those
 * in later. All functions are deterministic, allocation-free, and safe
 * to call from a self-test.
 */
#ifndef CREATION_OS_V51_AGENT_H
#define CREATION_OS_V51_AGENT_H

#include <stddef.h>

#include "cognitive_loop.h"

#ifdef __cplusplus
extern "C" {
#endif

#define V51_AGENT_MEMORY_MAX 64
#define V51_AGENT_NAME_MAX   48
#define V51_AGENT_REASON_MAX 96

typedef struct {
    char name[V51_AGENT_NAME_MAX];
    int  allowed;
} v51_tool_policy_t;

typedef struct {
    const v51_tool_policy_t *policies;
    int                      n_policies;
    float                    sigma_deny_above; /* deny any tool when σ ≥ this */
} v51_sandbox_t;

typedef struct {
    int   allowed;
    float sigma_observed;
    char  reason[V51_AGENT_REASON_MAX];
} v51_sandbox_decision_t;

typedef struct {
    char tool[V51_AGENT_NAME_MAX];
    int  success;
    float sigma_observed;
} v51_experience_t;

typedef struct {
    v51_experience_t buf[V51_AGENT_MEMORY_MAX];
    int              count;
    int              head; /* ring write pointer */
} v51_memory_t;

typedef struct {
    const char        *name;
    v51_sandbox_t      sandbox;
    v51_thresholds_t   thresholds;
    v51_memory_t       memory;
    int                max_steps;
    int                steps_taken;
    int                tools_executed;
    int                tools_denied;
    int                abstained;
    int                goal_reached;
} v51_agent_t;

void v51_agent_init(v51_agent_t *a, const char *name, int max_steps);
void v51_agent_default_sandbox(v51_sandbox_t *s);

v51_sandbox_decision_t v51_sandbox_check(const v51_sandbox_t *s,
                                         const char *tool_name,
                                         const sigma_decomposed_t *sigma);

void v51_memory_append(v51_memory_t *m, const v51_experience_t *e);

/**
 * Run up to `a->max_steps` scaffold steps toward `goal`. Returns the number
 * of steps taken. Honest semantics:
 *   - if the cognitive loop returns ACTION_ABSTAIN → stop and set
 *     `a->abstained = 1` (no "try anyway" loop)
 *   - each step can simulate at most one tool call; its sandbox decision
 *     is appended to memory
 *   - `goal_reached` is a stub predicate keyed off σ.total falling below
 *     `thresholds.answer_below`
 */
int v51_agent_run(v51_agent_t *a, const char *goal,
                  const float *logits, int n_logits,
                  const char *tool_to_try);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V51_AGENT_H */
