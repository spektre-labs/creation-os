/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v51 agent — scaffold implementation.
 */
#include "agent.h"

#include <string.h>

static void v51_copy_name(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void v51_agent_default_sandbox(v51_sandbox_t *s)
{
    if (!s) {
        return;
    }
    s->policies         = NULL;
    s->n_policies       = 0;
    s->sigma_deny_above = 0.75f;
}

void v51_agent_init(v51_agent_t *a, const char *name, int max_steps)
{
    if (!a) {
        return;
    }
    memset(a, 0, sizeof(*a));
    a->name = name;
    a->max_steps = (max_steps > 0) ? max_steps : 50;
    v51_agent_default_sandbox(&a->sandbox);
    v51_default_thresholds(&a->thresholds);
}

v51_sandbox_decision_t v51_sandbox_check(const v51_sandbox_t *s,
                                         const char *tool_name,
                                         const sigma_decomposed_t *sigma)
{
    v51_sandbox_decision_t d;
    memset(&d, 0, sizeof(d));
    d.sigma_observed = sigma ? sigma->total : 0.0f;

    if (!tool_name || !*tool_name) {
        strncpy(d.reason, "no tool name", sizeof(d.reason) - 1);
        d.allowed = 0;
        return d;
    }

    if (s && sigma && sigma->total >= s->sigma_deny_above) {
        strncpy(d.reason, "σ above sandbox deny threshold", sizeof(d.reason) - 1);
        d.allowed = 0;
        return d;
    }

    if (s && s->policies && s->n_policies > 0) {
        for (int i = 0; i < s->n_policies; i++) {
            if (strncmp(s->policies[i].name, tool_name, V51_AGENT_NAME_MAX) == 0) {
                d.allowed = s->policies[i].allowed ? 1 : 0;
                if (d.allowed) {
                    strncpy(d.reason, "policy allow", sizeof(d.reason) - 1);
                } else {
                    strncpy(d.reason, "policy deny", sizeof(d.reason) - 1);
                }
                return d;
            }
        }
        /* Unknown tool with policy list → deny by default (fail-closed). */
        strncpy(d.reason, "tool not in policy list (fail-closed)", sizeof(d.reason) - 1);
        d.allowed = 0;
        return d;
    }

    /* No policy list + σ under threshold → allow (scaffold default). */
    d.allowed = 1;
    strncpy(d.reason, "default allow under σ threshold", sizeof(d.reason) - 1);
    return d;
}

void v51_memory_append(v51_memory_t *m, const v51_experience_t *e)
{
    if (!m || !e) {
        return;
    }
    m->buf[m->head] = *e;
    m->head = (m->head + 1) % V51_AGENT_MEMORY_MAX;
    if (m->count < V51_AGENT_MEMORY_MAX) {
        m->count++;
    }
}

static int v51_goal_reached(const v51_agent_t *a,
                            const v51_cognitive_state_t *s)
{
    /* Stub predicate: goal is "reached" when σ drops below the answer
     * threshold AND the scaffold would answer immediately. */
    if (!a || !s) {
        return 0;
    }
    return (s->planned_action == V51_ACTION_ANSWER &&
            s->final_sigma.total < a->thresholds.answer_below) ? 1 : 0;
}

int v51_agent_run(v51_agent_t *a, const char *goal,
                  const float *logits, int n_logits,
                  const char *tool_to_try)
{
    if (!a) {
        return 0;
    }
    a->steps_taken   = 0;
    a->tools_executed = 0;
    a->tools_denied   = 0;
    a->abstained      = 0;
    a->goal_reached   = 0;

    for (int step = 0; step < a->max_steps; step++) {
        v51_cognitive_state_t s;
        v51_cognitive_step(goal, logits, n_logits, &a->thresholds, &s);
        a->steps_taken = step + 1;

        if (s.planned_action == V51_ACTION_ABSTAIN) {
            a->abstained = 1;
            return a->steps_taken;
        }

        if (tool_to_try && *tool_to_try) {
            v51_sandbox_decision_t d =
                v51_sandbox_check(&a->sandbox, tool_to_try, &s.final_sigma);

            v51_experience_t exp;
            memset(&exp, 0, sizeof(exp));
            v51_copy_name(exp.tool, sizeof(exp.tool), tool_to_try);
            exp.sigma_observed = d.sigma_observed;
            exp.success = d.allowed ? 1 : 0;
            v51_memory_append(&a->memory, &exp);

            if (d.allowed) {
                a->tools_executed++;
            } else {
                a->tools_denied++;
            }
        }

        if (v51_goal_reached(a, &s)) {
            a->goal_reached = 1;
            return a->steps_taken;
        }
    }

    return a->steps_taken;
}
