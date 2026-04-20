/*
 * cos agent — autonomous task execution CLI (A6).
 *
 * Composes σ-Tool + σ-Plan + σ-Agent to take a single task and
 * either dry-run the plan or execute it step by step, printing a
 * machine-readable receipt.
 *
 * This binary intentionally ships with a deterministic stub tool
 * executor: every tool call returns σ_result=0.05 and "ok", unless
 * the task string embeds a ":fail" or ":noisy" marker.  That keeps
 * the CLI pinnable in smoke tests and interoperable with the
 * existing stub generator pattern used by cos chat / cos benchmark.
 *
 * Task grammar (v0):
 *
 *   list-and-count:<dir>   → 2-step plan (file_read . + calculator)
 *   read:<path>            → 1-step plan (file_read path)
 *   fetch:<url>            → 1-step plan (web_fetch url)
 *   rm:<path>              → 1-step plan (shell_rm path, CONFIRM gate)
 *   noisy:<anything>       → 1-step plan (file_read) with σ_post=0.90
 *   fail:<anything>        → 1-step plan (file_read) whose executor rc=-1
 *   (anything else)        → no plan (BLOCK)
 *
 * Flags:
 *
 *   --dry-run              show the plan, do not execute
 *   --approve-each         treat CONFIRM as approved
 *   --max-steps  <N>       cap plan length
 *   --max-cost   <eur>     cap cumulative cost
 *   --local-only           reject NETWORK tools (web_fetch)
 *   --json                 emit one JSON object with receipts
 *   --banner-only          print banner + config; no plan
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "plan.h"
#include "tool.h"
#include "agent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *decision_name(cos_agent_decision_t d) {
    switch (d) {
        case COS_AGENT_ALLOW:   return "ALLOW";
        case COS_AGENT_CONFIRM: return "CONFIRM";
        case COS_AGENT_BLOCK:   return "BLOCK";
        default:                return "?";
    }
}

static const char *plan_status_name(cos_plan_status_t s) {
    switch (s) {
        case COS_PLAN_OK:         return "OK";
        case COS_PLAN_REPLAN:     return "REPLAN";
        case COS_PLAN_ABORTED:    return "ABORTED";
        case COS_PLAN_OVERBUDGET: return "OVERBUDGET";
        case COS_PLAN_BLOCKED:    return "BLOCKED";
        default:                  return "?";
    }
}

/* --- stub executor --------------------------------------------- */

typedef struct {
    int noisy_on_any;  /* σ_post=0.90 every call */
    int fail_on_any;   /* rc=-1 on every call    */
} exec_ctx_t;

static int stub_exec(const cos_tool_call_t *call, void *ctx_void,
                     const char **out_text, float *out_sigma_result)
{
    exec_ctx_t *ctx = (exec_ctx_t *)ctx_void;
    (void)call;
    if (ctx && ctx->fail_on_any)  { *out_text = ""; *out_sigma_result = 1.0f; return -1; }
    if (ctx && ctx->noisy_on_any) { *out_text = "ok"; *out_sigma_result = 0.90f; return 0; }
    *out_text = "ok";
    *out_sigma_result = 0.05f;
    return 0;
}

/* --- tool registry --------------------------------------------- */

static void bootstrap_tools(cos_tool_registry_t *reg, cos_tool_t *slots, int cap) {
    cos_sigma_tool_registry_init(reg, slots, cap);
    cos_tool_t calc = {.name = "calculator", .description = "math",
                       .risk = COS_TOOL_SAFE,    .cost_eur = 0.00001};
    cos_tool_t fread_ = {.name = "file_read",  .description = "read a file",
                       .risk = COS_TOOL_READ,    .cost_eur = 0.00005};
    cos_tool_t fetch = {.name = "web_fetch",  .description = "fetch a URL",
                       .risk = COS_TOOL_NETWORK, .cost_eur = 0.00100};
    cos_tool_t rm_   = {.name = "shell_rm",   .description = "delete files",
                       .risk = COS_TOOL_IRREVERSIBLE, .cost_eur = 0.0};
    cos_sigma_tool_register(reg, &calc);
    cos_sigma_tool_register(reg, &fread_);
    cos_sigma_tool_register(reg, &fetch);
    cos_sigma_tool_register(reg, &rm_);
}

/* --- task → plan compilation ------------------------------------ */

typedef enum {
    COS_TASK_LIST_AND_COUNT,
    COS_TASK_READ,
    COS_TASK_FETCH,
    COS_TASK_RM,
    COS_TASK_NOISY,
    COS_TASK_FAIL,
    COS_TASK_UNKNOWN,
} task_kind_t;

static task_kind_t classify_task(const char *task) {
    if (!task) return COS_TASK_UNKNOWN;
    if (strncmp(task, "list-and-count:", 15) == 0) return COS_TASK_LIST_AND_COUNT;
    if (strncmp(task, "read:",  5) == 0) return COS_TASK_READ;
    if (strncmp(task, "fetch:", 6) == 0) return COS_TASK_FETCH;
    if (strncmp(task, "rm:",    3) == 0) return COS_TASK_RM;
    if (strncmp(task, "noisy:", 6) == 0) return COS_TASK_NOISY;
    if (strncmp(task, "fail:",  5) == 0) return COS_TASK_FAIL;
    return COS_TASK_UNKNOWN;
}

static int compile_plan(cos_plan_t *plan, const cos_tool_registry_t *reg,
                        task_kind_t kind, const char *task)
{
    float ss;
    cos_tool_call_t c_read  = cos_sigma_tool_select(reg, "file_read target", task, &ss);
    cos_tool_call_t c_calc  = cos_sigma_tool_select(reg, "calculator sum",    "1+1", &ss);
    cos_tool_call_t c_fetch = cos_sigma_tool_select(reg, "web_fetch url",     task, &ss);
    cos_tool_call_t c_rm    = cos_sigma_tool_select(reg, "shell_rm  path",    task, &ss);

    switch (kind) {
        case COS_TASK_LIST_AND_COUNT:
            if (cos_sigma_plan_add_step(plan, "list directory", c_read, 0.05f) != 0) return -1;
            if (cos_sigma_plan_add_step(plan, "count lines",    c_calc, 0.08f) != 0) return -2;
            return 0;
        case COS_TASK_READ:
            if (cos_sigma_plan_add_step(plan, "read file", c_read, 0.05f) != 0) return -1;
            return 0;
        case COS_TASK_FETCH:
            if (cos_sigma_plan_add_step(plan, "fetch url", c_fetch, 0.10f) != 0) return -1;
            return 0;
        case COS_TASK_RM:
            if (cos_sigma_plan_add_step(plan, "remove path", c_rm, 0.05f) != 0) return -1;
            return 0;
        case COS_TASK_NOISY:
            if (cos_sigma_plan_add_step(plan, "noisy read", c_read, 0.05f) != 0) return -1;
            return 0;
        case COS_TASK_FAIL:
            if (cos_sigma_plan_add_step(plan, "failing read", c_read, 0.05f) != 0) return -1;
            return 0;
        default: return -9;
    }
}

/* --- main ------------------------------------------------------- */

int main(int argc, char **argv) {
    /* Defaults. */
    int dry_run = 0, approve_each = 0, local_only = 0, json_mode = 0;
    int banner_only = 0;
    int max_steps = 8;
    double max_cost = 0.01;
    const char *task = NULL;

    for (int i = 1; i < argc; ++i) {
        if      (strcmp(argv[i], "--dry-run")      == 0) dry_run = 1;
        else if (strcmp(argv[i], "--approve-each") == 0) approve_each = 1;
        else if (strcmp(argv[i], "--local-only")   == 0) local_only = 1;
        else if (strcmp(argv[i], "--json")         == 0) json_mode = 1;
        else if (strcmp(argv[i], "--banner-only")  == 0) banner_only = 1;
        else if (strcmp(argv[i], "--max-steps")    == 0 && i + 1 < argc)
            max_steps = atoi(argv[++i]);
        else if (strcmp(argv[i], "--max-cost")     == 0 && i + 1 < argc)
            max_cost = atof(argv[++i]);
        else if (argv[i][0] != '-' && !task) task = argv[i];
    }
    if (max_steps < 1) max_steps = 1;
    if (max_steps > 8) max_steps = 8;
    if (max_cost  < 0) max_cost  = 0.0;

    if (banner_only) {
        printf("{\"cli\":\"cos-agent\",\"banner\":true,"
               "\"max_steps\":%d,\"max_cost_eur\":%.4f,"
               "\"local_only\":%s,\"approve_each\":%s,"
               "\"dry_run\":%s}\n",
               max_steps, max_cost,
               local_only ? "true" : "false",
               approve_each ? "true" : "false",
               dry_run ? "true" : "false");
        return 0;
    }

    if (!task) {
        fprintf(stderr, "usage: cos agent [--dry-run] [--approve-each] [--local-only]\n"
                        "                 [--max-steps N] [--max-cost EUR] [--json]\n"
                        "                 \"<task>\"\n");
        return 2;
    }

    /* Bootstrap. */
    cos_tool_registry_t reg; cos_tool_t slots[8];
    bootstrap_tools(&reg, slots, 8);
    /* approve-each means the human is in the loop for every CONFIRM,
     * so we run with max base autonomy — the gate reaches the CONFIRM
     * band for IRREVERSIBLE actions and the CLI converts CONFIRM to
     * ALLOW via `confirmed=1`.  Without approve-each we default to a
     * conservative 0.80 so accidental BLOCKs are common, not rare. */
    cos_sigma_agent_t ag;
    cos_sigma_agent_init(&ag, approve_each ? 1.00f : 0.80f, 0.10f);

    /* Default max_risk accepts every tool the registry exposes; the
     * σ-Tool gate (IRREVERSIBLE force-downgrade to CONFIRM) and
     * --approve-each together decide whether a high-risk step ever
     * actually runs.  --local-only clamps the plan compiler so it
     * rejects NETWORK + IRREVERSIBLE outright.                      */
    cos_tool_risk_t max_risk = local_only ? COS_TOOL_WRITE : COS_TOOL_IRREVERSIBLE;

    cos_plan_t plan;
    cos_plan_step_t storage[8];
    cos_plan_budget_t budget = {
        .max_steps    = max_steps,
        .max_cost_eur = max_cost,
        .max_risk     = max_risk,
        .tau_replan   = 0.60f,
    };
    if (cos_sigma_plan_init(&plan, storage, 8, task, budget) != 0) {
        fprintf(stderr, "cos-agent: plan init failed\n");
        return 3;
    }

    task_kind_t kind = classify_task(task);
    int compile_rc = compile_plan(&plan, &reg, kind, task);

    /* Execution context: noisy / fail mapped to stub flags. */
    exec_ctx_t ectx = {0};
    if (kind == COS_TASK_NOISY) ectx.noisy_on_any = 1;
    if (kind == COS_TASK_FAIL)  ectx.fail_on_any  = 1;

    const char *texts[8] = {0};
    cos_plan_status_t st = COS_PLAN_OK;
    if (!dry_run) {
        st = cos_sigma_plan_execute(&plan, &ag, stub_exec, &ectx,
                                    approve_each ? 1 : 0, texts);
    }

    /* Output. */
    if (json_mode) {
        printf("{\"cli\":\"cos-agent\","
               "\"task\":\"%s\","
               "\"config\":{\"max_steps\":%d,\"max_cost_eur\":%.4f,"
                          "\"local_only\":%s,\"approve_each\":%s,\"dry_run\":%s},"
               "\"plan\":{\"compile_rc\":%d,\"n_steps\":%d,\"sigma_plan\":%.4f,"
                        "\"status\":\"%s\",\"replan_at\":%d,"
                        "\"spent_eur\":%.6f},\"steps\":[",
               task, max_steps, max_cost,
               local_only ? "true" : "false",
               approve_each ? "true" : "false",
               dry_run ? "true" : "false",
               compile_rc, plan.count, (double)plan.sigma_plan,
               plan_status_name(st), plan.replan_at,
               plan.spent_eur);
        for (int i = 0; i < plan.count; ++i) {
            cos_plan_step_t *sp = &plan.steps[i];
            printf("%s{\"i\":%d,\"desc\":\"%s\",\"tool\":\"%s\","
                   "\"risk\":%d,\"sigma_pre\":%.4f,\"sigma_post\":%.4f,"
                   "\"decision\":\"%s\",\"rc\":%d,\"done\":%s}",
                   (i ? "," : ""), i,
                   sp->description ? sp->description : "",
                   sp->call.tool ? sp->call.tool->name : "null",
                   sp->call.tool ? (int)sp->call.tool->risk : -1,
                   (double)sp->sigma_pre, (double)sp->sigma_post,
                   decision_name(sp->call.decision),
                   sp->call.rc,
                   sp->completed ? "true" : "false");
        }
        printf("]}\n");
    } else {
        fprintf(stdout, "cos agent · task: %s\n", task);
        fprintf(stdout, "  budget:  max_steps=%d  max_cost=€%.4f  %s  %s\n",
                max_steps, max_cost,
                local_only ? "LOCAL-ONLY" : "HYBRID",
                approve_each ? "(approve-each)" : "(auto)");
        fprintf(stdout, "  plan:    n_steps=%d  σ_plan=%.3f  status=%s\n",
                plan.count, (double)plan.sigma_plan, plan_status_name(st));
        for (int i = 0; i < plan.count; ++i) {
            cos_plan_step_t *sp = &plan.steps[i];
            fprintf(stdout, "    [%d] %-16s  tool=%-11s risk=%d  σ_pre=%.2f  σ_post=%.2f  %s  rc=%d\n",
                    i,
                    sp->description ? sp->description : "",
                    sp->call.tool ? sp->call.tool->name : "null",
                    sp->call.tool ? (int)sp->call.tool->risk : -1,
                    (double)sp->sigma_pre, (double)sp->sigma_post,
                    decision_name(sp->call.decision),
                    sp->call.rc);
        }
        fprintf(stdout, "  spent:   €%.6f\n", plan.spent_eur);
    }
    /* Exit code: 0 on OK plan, 1 on REPLAN/BLOCKED, 2 on ABORTED/OVERBUDGET,
     * 3 on unknown task. */
    if (compile_rc == -9)               return 3;
    if (dry_run)                        return 0;
    if (st == COS_PLAN_OK)              return 0;
    if (st == COS_PLAN_REPLAN || st == COS_PLAN_BLOCKED) return 1;
    return 2;
}
