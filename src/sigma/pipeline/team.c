/*
 * σ-multi-agent orchestration — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "team.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------- Helpers -------------------- */

static void tcpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static uint64_t team_fnv1a(const void *buf, size_t len) {
    const unsigned char *p = (const unsigned char *)buf;
    uint64_t h = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

static float clip01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/* Stable [0,1) floats derived from a 64-bit seed. */
static float seed_to_unit(uint64_t seed) {
    return (float)((double)(seed >> 11) / (double)((uint64_t)1 << 53));
}

/* -------------------- Lifecycle -------------------- */

void cos_team_init(cos_team_t *team, const char *coordinator_id) {
    if (!team) return;
    memset(team, 0, sizeof *team);
    tcpy(team->coordinator_id, sizeof team->coordinator_id,
         coordinator_id ? coordinator_id : "coordinator");
    team->tau_retry = COS_TEAM_TAU_RETRY_DEFAULT;
    team->tau_abort = COS_TEAM_TAU_ABORT_DEFAULT;
}

int cos_team_add_agent(cos_team_t *team,
                       const char *id, const char *role,
                       float capability) {
    if (!team || !id || !role) return COS_TEAM_ERR_ARG;
    if (team->n_agents >= COS_TEAM_MAX_AGENTS) return COS_TEAM_ERR_FULL;
    cos_team_agent_t *a = &team->agents[team->n_agents];
    tcpy(a->id,   sizeof a->id,   id);
    tcpy(a->role, sizeof a->role, role);
    a->capability = clip01(capability);
    a->uses = 0;
    return team->n_agents++;
}

void cos_team_set_thresholds(cos_team_t *team, float tau_retry, float tau_abort) {
    if (!team) return;
    team->tau_retry = tau_retry;
    team->tau_abort = tau_abort;
}

/* -------------------- Planning -------------------- */

/* Canonical 5-step plan.  Keeping the plan structural (same shape
 * for every goal) makes σ measurements comparable across tasks
 * and keeps the CI fixture deterministic. */
static const struct {
    const char *role;
    const char *template;
} PLAN_TEMPLATE[COS_TEAM_MAX_STEPS] = {
    { "researcher", "Gather relevant sources for: %s" },
    { "coordinator","Design a stepwise approach for: %s" },
    { "coder",      "Implement the core of: %s" },
    { "reviewer",   "Review the implementation of: %s" },
    { "writer",     "Document the outcome of: %s" },
    /* remaining entries left blank */
};

int cos_team_plan(cos_team_t *team,
                  const char *goal,
                  cos_team_plan_t *plan) {
    if (!team || !goal || !plan) return COS_TEAM_ERR_ARG;
    memset(plan, 0, sizeof *plan);
    tcpy(plan->goal, sizeof plan->goal, goal);

    int n = 5;
    plan->n_steps = n;
    for (int i = 0; i < n; i++) {
        cos_team_step_t *s = &plan->steps[i];
        s->index = i;
        tcpy(s->required_role, sizeof s->required_role, PLAN_TEMPLATE[i].role);
        snprintf(s->description, sizeof s->description,
                 PLAN_TEMPLATE[i].template, goal);
        s->sigma      = 1.0f;
        s->attempts   = 0;
        s->escalated  = 0;
        s->assigned[0] = '\0';
    }
    return COS_TEAM_OK;
}

/* -------------------- Agent selection -------------------- */

/* Comparator: highest capability wins; ties broken by id
 * lexicographic order to keep selection deterministic. */
static int cand_cmp(const void *a, const void *b) {
    const cos_team_agent_t *x = *(const cos_team_agent_t *const *)a;
    const cos_team_agent_t *y = *(const cos_team_agent_t *const *)b;
    if (x->capability < y->capability) return  1;
    if (x->capability > y->capability) return -1;
    return strcmp(x->id, y->id);
}

/* Populate `out` with candidates for `role`, sorted best-first.
 * Returns the count. */
static int gather_candidates(cos_team_t *team,
                             const char *role,
                             const cos_team_agent_t **out,
                             int cap) {
    int k = 0;
    for (int i = 0; i < team->n_agents && k < cap; i++) {
        if (strcmp(team->agents[i].role, role) == 0) {
            out[k++] = &team->agents[i];
        }
    }
    qsort(out, (size_t)k, sizeof *out, cand_cmp);
    return k;
}

/* -------------------- Built-in executor -------------------- */

/* Deterministic σ from (goal, step, agent, attempt, capability).
 * Logic:
 *   base σ = 1 - capability  (0.2 for a 0.8 agent)
 *   + hash-derived noise in [-0.10, +0.10]
 *   - retry bonus (repeated attempts slightly improve σ)
 *   clipped to [0,1].
 * This is a placeholder for a real runtime pipeline (which the
 * caller supplies via cos_team_exec_fn).
 */
float cos_team_exec_sim(const cos_team_plan_t *plan,
                        const cos_team_step_t *step,
                        const cos_team_agent_t *agent,
                        int attempt,
                        void *user) {
    (void)user;
    if (!plan || !step || !agent) return 1.0f;
    uint64_t h = team_fnv1a(plan->goal,        strlen(plan->goal));
    h ^= team_fnv1a(step->description,         strlen(step->description));
    h ^= team_fnv1a(agent->id,                 strlen(agent->id));
    h ^= (uint64_t)(step->index + 1) * 0xA3B1C2D3E4F5061FULL;
    h ^= (uint64_t)(attempt  + 1) * 0x1F5E4D3C2B1A0918ULL;
    float u    = seed_to_unit(h);
    float base = 1.0f - agent->capability;
    float noise = (u - 0.5f) * 0.20f;
    float retry_bonus = (float)attempt * 0.08f;
    return clip01(base + noise - retry_bonus);
}

/* -------------------- Execution -------------------- */

int cos_team_run(cos_team_t *team,
                 cos_team_plan_t *plan,
                 cos_team_exec_fn exec,
                 void *user) {
    if (!team || !plan) return COS_TEAM_ERR_ARG;
    if (!exec) exec = cos_team_exec_sim;

    for (int i = 0; i < team->n_agents; i++) team->agents[i].uses = 0;

    int escalations = 0;
    double sigma_sum = 0.0;

    for (int i = 0; i < plan->n_steps; i++) {
        cos_team_step_t *s = &plan->steps[i];
        const cos_team_agent_t *cands[COS_TEAM_MAX_AGENTS];
        int n = gather_candidates(team, s->required_role, cands, COS_TEAM_MAX_AGENTS);

        float best_sigma = 1.0f;
        const cos_team_agent_t *best_agent = NULL;
        int tried = 0;

        if (n == 0) {
            s->escalated = 1;
            s->sigma     = 1.0f;
            s->attempts  = 0;
            s->assigned[0] = '\0';
            escalations++;
            sigma_sum += 1.0;
            continue;
        }

        int limit = n < (COS_TEAM_MAX_RETRIES + 1) ? n : (COS_TEAM_MAX_RETRIES + 1);
        for (int a = 0; a < limit; a++) {
            tried++;
            float sigma = exec(plan, s, cands[a], a, user);
            if (sigma < best_sigma) {
                best_sigma = sigma;
                best_agent = cands[a];
            }
            if (sigma <= team->tau_retry) break;  /* good enough */
        }

        s->attempts = tried;
        s->sigma    = best_sigma;
        if (best_agent) {
            tcpy(s->assigned, sizeof s->assigned, best_agent->id);
            for (int k = 0; k < team->n_agents; k++) {
                if (&team->agents[k] == best_agent) { team->agents[k].uses++; break; }
            }
        }

        if (best_sigma > team->tau_abort) {
            s->escalated = 1;
            escalations++;
        } else {
            s->escalated = 0;
        }
        sigma_sum += best_sigma;
    }

    plan->escalations = escalations;
    plan->sigma_goal  = plan->n_steps > 0
        ? (float)(sigma_sum / (double)plan->n_steps)
        : 1.0f;

    return escalations ? COS_TEAM_FAIL : COS_TEAM_OK;
}

/* -------------------- Self-test -------------------- */

int cos_team_self_test(void) {
    cos_team_t team;
    cos_team_init(&team, "coordinator");
    if (cos_team_add_agent(&team, "alice",   "coordinator", 0.85f) < 0) return -1;
    if (cos_team_add_agent(&team, "rosa",    "researcher",  0.90f) < 0) return -2;
    if (cos_team_add_agent(&team, "rami",    "researcher",  0.60f) < 0) return -3;
    if (cos_team_add_agent(&team, "cody",    "coder",       0.88f) < 0) return -4;
    if (cos_team_add_agent(&team, "rex",     "reviewer",    0.92f) < 0) return -5;
    if (cos_team_add_agent(&team, "willow",  "writer",      0.80f) < 0) return -6;

    cos_team_plan_t plan;
    if (cos_team_plan(&team, "Build a REST API for todo list", &plan) != COS_TEAM_OK)
        return -7;
    if (plan.n_steps != 5) return -8;

    if (cos_team_run(&team, &plan, NULL, NULL) != COS_TEAM_OK) return -9;
    if (plan.sigma_goal > team.tau_abort)  return -10;

    /* Determinism: second run with same inputs produces identical output. */
    cos_team_plan_t plan2;
    cos_team_plan(&team, "Build a REST API for todo list", &plan2);
    cos_team_run(&team, &plan2, NULL, NULL);
    if (plan2.n_steps != plan.n_steps) return -11;
    for (int i = 0; i < plan.n_steps; i++) {
        if (plan.steps[i].sigma != plan2.steps[i].sigma) return -12;
        if (strcmp(plan.steps[i].assigned, plan2.steps[i].assigned) != 0) return -13;
    }

    /* Missing-role case: no writer → escalation. */
    cos_team_t empty;
    cos_team_init(&empty, "coordinator");
    cos_team_add_agent(&empty, "alice", "coordinator", 0.85f);
    cos_team_add_agent(&empty, "rosa",  "researcher",  0.90f);
    cos_team_add_agent(&empty, "cody",  "coder",       0.88f);
    cos_team_add_agent(&empty, "rex",   "reviewer",    0.92f);
    cos_team_plan_t plan3;
    cos_team_plan(&empty, "doc only", &plan3);
    int rc = cos_team_run(&empty, &plan3, NULL, NULL);
    if (rc != COS_TEAM_FAIL) return -14;
    if (plan3.escalations < 1)  return -15;
    return 0;
}
