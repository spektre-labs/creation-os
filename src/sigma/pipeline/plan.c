/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Plan primitive (budgeted multi-step planning + execute). */

#include "plan.h"

#include <string.h>

int cos_sigma_plan_init(cos_plan_t *plan, cos_plan_step_t *storage, int cap,
                        const char *goal, cos_plan_budget_t budget)
{
    if (!plan || !storage || cap <= 0) return -1;
    if (budget.max_steps <= 0 || budget.max_steps > cap) return -2;
    if (!(budget.max_cost_eur >= 0.0)) return -3;
    if (!(budget.tau_replan > 0.0f && budget.tau_replan <= 1.0f)) return -4;
    memset(plan, 0, sizeof *plan);
    memset(storage, 0, sizeof(cos_plan_step_t) * (size_t)cap);
    plan->goal      = goal;
    plan->steps     = storage;
    plan->cap       = cap;
    plan->budget    = budget;
    plan->sigma_plan = 0.0f;
    plan->status    = COS_PLAN_OK;
    plan->replan_at = -1;
    return 0;
}

int cos_sigma_plan_add_step(cos_plan_t *plan, const char *description,
                            cos_tool_call_t call, float sigma_pre)
{
    if (!plan) return -4;
    if (!call.tool) return -4;
    if (!(sigma_pre >= 0.0f && sigma_pre <= 1.0f)) return -5;
    if (plan->count >= plan->cap || plan->count >= plan->budget.max_steps)
        return -1;
    if ((int)call.tool->risk > (int)plan->budget.max_risk)
        return -2;
    double projected = plan->spent_eur + call.tool->cost_eur;
    if (projected > plan->budget.max_cost_eur)
        return -3;

    cos_plan_step_t *s = &plan->steps[plan->count++];
    s->description = description;
    s->call        = call;
    s->sigma_pre   = sigma_pre;
    s->sigma_post  = 1.0f;
    s->completed   = 0;
    s->cost_eur    = 0.0;
    plan->spent_eur = projected;  /* commit projected cost up front */
    cos_sigma_plan_recompute_sigma(plan);
    return 0;
}

float cos_sigma_plan_recompute_sigma(cos_plan_t *plan) {
    if (!plan || plan->count <= 0) { if (plan) plan->sigma_plan = 1.0f; return 1.0f; }
    double sum = 0.0;
    for (int i = 0; i < plan->count; ++i) sum += (double)plan->steps[i].sigma_pre;
    float mean = (float)(sum / (double)plan->count);
    /* Penalty for brushing the step budget. */
    float frac = (float)plan->count / (float)plan->budget.max_steps;
    float penalty = 0.0f;
    if (frac > 0.80f) penalty = (frac - 0.80f) * 0.5f;
    float s = mean + penalty;
    if (s > 1.0f) s = 1.0f;
    plan->sigma_plan = s;
    return s;
}

cos_plan_status_t
cos_sigma_plan_execute(cos_plan_t *plan,
                       const cos_sigma_agent_t *agent,
                       cos_tool_executor_fn executor,
                       void *executor_ctx,
                       int confirm_all,
                       const char **out_texts)
{
    if (!plan || !executor) return COS_PLAN_ABORTED;
    plan->status    = COS_PLAN_OK;
    plan->replan_at = -1;

    for (int i = 0; i < plan->count; ++i) {
        cos_plan_step_t *s = &plan->steps[i];
        const char *text = NULL;
        int rc = cos_sigma_tool_call(&s->call, agent, executor,
                                     executor_ctx, confirm_all, &text);
        /* Gate-level stops key off the decision that cos_sigma_tool_call
         * stamps on the call record; this keeps executor-rc collisions
         * (rc == -1 is a perfectly reasonable executor failure) from
         * being misreported as gate BLOCKs.                         */
        if (s->call.decision == COS_AGENT_BLOCK) {
            plan->status    = COS_PLAN_BLOCKED;
            plan->replan_at = i;
            if (out_texts) out_texts[i] = NULL;
            return COS_PLAN_BLOCKED;
        }
        if (s->call.decision == COS_AGENT_CONFIRM && rc == -2) {
            plan->status    = COS_PLAN_BLOCKED;
            plan->replan_at = i;
            if (out_texts) out_texts[i] = NULL;
            return COS_PLAN_BLOCKED;
        }
        if (out_texts) out_texts[i] = text;
        s->sigma_post = s->call.sigma_result;
        s->completed  = (rc == 0);
        s->cost_eur   = s->call.tool ? s->call.tool->cost_eur : 0.0;
        if (rc != 0) {
            plan->status    = COS_PLAN_ABORTED;
            plan->replan_at = i;
            return COS_PLAN_ABORTED;
        }
        /* Budget check at realized cost. */
        if (plan->spent_eur > plan->budget.max_cost_eur) {
            plan->status    = COS_PLAN_OVERBUDGET;
            plan->replan_at = i;
            return COS_PLAN_OVERBUDGET;
        }
        /* Replan trigger. */
        if (s->sigma_post > plan->budget.tau_replan) {
            plan->status    = COS_PLAN_REPLAN;
            plan->replan_at = i;
            return COS_PLAN_REPLAN;
        }
    }
    plan->status = COS_PLAN_OK;
    return COS_PLAN_OK;
}

/* ---------------- self-test ------------------------------------ */

static int exec_ok(const cos_tool_call_t *call, void *ctx,
                   const char **out_text, float *out_sigma_result) {
    (void)call; (void)ctx;
    *out_text = "ok";
    *out_sigma_result = 0.05f;
    return 0;
}

static int exec_noisy_on_third(const cos_tool_call_t *call, void *ctx,
                               const char **out_text, float *out_sigma_result) {
    int *n = (int *)ctx;
    (*n)++;
    (void)call;
    *out_text = "ok";
    *out_sigma_result = (*n == 3) ? 0.90f : 0.05f;
    return 0;
}

static int exec_fail(const cos_tool_call_t *call, void *ctx,
                     const char **out_text, float *out_sigma_result) {
    (void)call; (void)ctx;
    *out_text = "";
    *out_sigma_result = 1.0f;
    return -9;
}

static int check_init_and_budget(void) {
    cos_plan_t p;
    cos_plan_step_t s[4];
    cos_plan_budget_t b = {.max_steps = 4, .max_cost_eur = 0.01,
                           .max_risk = COS_TOOL_WRITE, .tau_replan = 0.60f};
    if (cos_sigma_plan_init(NULL, s, 4, "g", b) == 0) return 10;
    if (cos_sigma_plan_init(&p, NULL, 4, "g", b) == 0) return 11;
    if (cos_sigma_plan_init(&p, s, 0, "g", b)   == 0) return 12;
    /* invalid budget */
    cos_plan_budget_t bad = b; bad.tau_replan = 0.0f;
    if (cos_sigma_plan_init(&p, s, 4, "g", bad) == 0) return 13;
    if (cos_sigma_plan_init(&p, s, 4, "g", b)   != 0) return 14;
    return 0;
}

static void mk_tools(cos_tool_registry_t *reg, cos_tool_t *slots,
                     int cap, cos_tool_t *net, cos_tool_t *rm)
{
    cos_sigma_tool_registry_init(reg, slots, cap);
    cos_tool_t calc = {.name = "calculator", .description = "math",
                       .risk = COS_TOOL_SAFE,    .cost_eur = 0.001};
    cos_tool_t read_t = {.name = "file_read", .description = "read",
                       .risk = COS_TOOL_READ,    .cost_eur = 0.001};
    *net = (cos_tool_t){.name = "web_fetch", .description = "fetch",
                        .risk = COS_TOOL_NETWORK, .cost_eur = 0.005};
    *rm  = (cos_tool_t){.name = "shell_rm",  .description = "delete",
                        .risk = COS_TOOL_IRREVERSIBLE, .cost_eur = 0.0};
    cos_sigma_tool_register(reg, &calc);
    cos_sigma_tool_register(reg, &read_t);
    cos_sigma_tool_register(reg, net);
    cos_sigma_tool_register(reg, rm);
}

static int check_add_steps_budget(void) {
    cos_plan_t p;
    cos_plan_step_t s[4];
    cos_plan_budget_t b = {.max_steps = 4, .max_cost_eur = 0.003,
                           .max_risk = COS_TOOL_READ, .tau_replan = 0.60f};
    if (cos_sigma_plan_init(&p, s, 4, "listfiles", b) != 0) return 20;
    cos_tool_registry_t reg; cos_tool_t rslots[8], netT, rmT;
    mk_tools(&reg, rslots, 8, &netT, &rmT);

    float ss;
    cos_tool_call_t c_read = cos_sigma_tool_select(&reg, "file_read .", ".",  &ss);
    cos_tool_call_t c_net  = cos_sigma_tool_select(&reg, "web_fetch http", "http", &ss);
    cos_tool_call_t c_rm   = cos_sigma_tool_select(&reg, "shell_rm  a",    "a",   &ss);

    /* Two reads fit (€0.002 total). */
    if (cos_sigma_plan_add_step(&p, "read1", c_read, 0.05f) != 0) return 21;
    if (cos_sigma_plan_add_step(&p, "read2", c_read, 0.05f) != 0) return 22;
    /* Third read would be €0.003 (== max) → OK; fourth would be €0.004 → -3. */
    if (cos_sigma_plan_add_step(&p, "read3", c_read, 0.05f) != 0) return 23;
    if (cos_sigma_plan_add_step(&p, "read4", c_read, 0.05f) != -3) return 24;
    /* NETWORK > max_risk (READ) → -2. */
    if (cos_sigma_plan_add_step(&p, "fetch", c_net, 0.05f) != -2) return 25;
    /* IRREVERSIBLE > max_risk → -2. */
    if (cos_sigma_plan_add_step(&p, "rm",    c_rm,  0.05f) != -2) return 26;
    /* σ out of range → -5. */
    cos_plan_t p2;
    cos_plan_step_t s2[4];
    cos_sigma_plan_init(&p2, s2, 4, "g", b);
    if (cos_sigma_plan_add_step(&p2, "bad", c_read, 1.5f) != -5) return 27;
    /* NULL tool call → -4. */
    cos_tool_call_t c_null; memset(&c_null, 0, sizeof c_null);
    if (cos_sigma_plan_add_step(&p2, "n", c_null, 0.05f) != -4) return 28;
    return 0;
}

static int check_execute_happy(void) {
    cos_plan_t p;
    cos_plan_step_t s[4];
    cos_plan_budget_t b = {.max_steps = 4, .max_cost_eur = 0.01,
                           .max_risk = COS_TOOL_READ, .tau_replan = 0.60f};
    cos_sigma_plan_init(&p, s, 4, "g", b);
    cos_tool_registry_t reg; cos_tool_t rslots[8], netT, rmT;
    mk_tools(&reg, rslots, 8, &netT, &rmT);
    float ss;
    cos_tool_call_t c = cos_sigma_tool_select(&reg, "file_read .", ".", &ss);
    cos_sigma_plan_add_step(&p, "a", c, 0.05f);
    cos_sigma_plan_add_step(&p, "b", c, 0.05f);
    cos_sigma_plan_add_step(&p, "c", c, 0.05f);

    cos_sigma_agent_t ag; cos_sigma_agent_init(&ag, 0.90f, 0.10f);
    const char *out[4];
    cos_plan_status_t st = cos_sigma_plan_execute(&p, &ag, exec_ok, NULL, 0, out);
    if (st != COS_PLAN_OK) return 30;
    for (int i = 0; i < p.count; ++i) {
        if (!p.steps[i].completed) return 31 + i;
        if (p.steps[i].sigma_post > 0.10f) return 35 + i;
    }
    return 0;
}

static int check_execute_replan(void) {
    cos_plan_t p;
    cos_plan_step_t s[4];
    cos_plan_budget_t b = {.max_steps = 4, .max_cost_eur = 0.01,
                           .max_risk = COS_TOOL_READ, .tau_replan = 0.60f};
    cos_sigma_plan_init(&p, s, 4, "g", b);
    cos_tool_registry_t reg; cos_tool_t rslots[8], netT, rmT;
    mk_tools(&reg, rslots, 8, &netT, &rmT);
    float ss;
    cos_tool_call_t c = cos_sigma_tool_select(&reg, "file_read .", ".", &ss);
    for (int i = 0; i < 4; ++i) cos_sigma_plan_add_step(&p, "x", c, 0.05f);

    cos_sigma_agent_t ag; cos_sigma_agent_init(&ag, 0.90f, 0.10f);
    int n = 0;
    const char *out[4];
    cos_plan_status_t st = cos_sigma_plan_execute(&p, &ag, exec_noisy_on_third,
                                                  &n, 0, out);
    if (st != COS_PLAN_REPLAN) return 40;
    if (p.replan_at != 2) return 41;
    if (!p.steps[0].completed || !p.steps[1].completed) return 42;
    if (p.steps[3].completed) return 43;  /* shouldn't have run */
    return 0;
}

static int check_execute_abort(void) {
    cos_plan_t p;
    cos_plan_step_t s[4];
    cos_plan_budget_t b = {.max_steps = 4, .max_cost_eur = 0.01,
                           .max_risk = COS_TOOL_READ, .tau_replan = 0.60f};
    cos_sigma_plan_init(&p, s, 4, "g", b);
    cos_tool_registry_t reg; cos_tool_t rslots[8], netT, rmT;
    mk_tools(&reg, rslots, 8, &netT, &rmT);
    float ss;
    cos_tool_call_t c = cos_sigma_tool_select(&reg, "file_read .", ".", &ss);
    cos_sigma_plan_add_step(&p, "a", c, 0.05f);
    cos_sigma_plan_add_step(&p, "b", c, 0.05f);

    cos_sigma_agent_t ag; cos_sigma_agent_init(&ag, 0.90f, 0.10f);
    const char *out[4];
    cos_plan_status_t st = cos_sigma_plan_execute(&p, &ag, exec_fail, NULL, 0, out);
    if (st != COS_PLAN_ABORTED) return 50;
    if (p.replan_at != 0) return 51;
    return 0;
}

int cos_sigma_plan_self_test(void) {
    int rc;
    if ((rc = check_init_and_budget()))  return rc;
    if ((rc = check_add_steps_budget())) return rc;
    if ((rc = check_execute_happy()))    return rc;
    if ((rc = check_execute_replan()))   return rc;
    if ((rc = check_execute_abort()))    return rc;
    return 0;
}
