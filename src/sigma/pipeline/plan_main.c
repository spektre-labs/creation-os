/* σ-Plan self-test binary + canonical multi-step demo. */

#include "plan.h"

#include <stdio.h>
#include <string.h>

static const char *status_name(cos_plan_status_t s) {
    switch (s) {
        case COS_PLAN_OK:         return "OK";
        case COS_PLAN_REPLAN:     return "REPLAN";
        case COS_PLAN_ABORTED:    return "ABORTED";
        case COS_PLAN_OVERBUDGET: return "OVERBUDGET";
        case COS_PLAN_BLOCKED:    return "BLOCKED";
        default:                  return "?";
    }
}

static int demo_exec(const cos_tool_call_t *call, void *ctx,
                     const char **out_text, float *out_sigma_result) {
    int *n = (int *)ctx;
    (*n)++;
    (void)call;
    *out_text = "ok";
    *out_sigma_result = 0.05f;
    return 0;
}

int main(int argc, char **argv) {
    int rc = cos_sigma_plan_self_test();

    cos_tool_registry_t reg;
    cos_tool_t rslots[8];
    cos_sigma_tool_registry_init(&reg, rslots, 8);
    cos_tool_t calc   = {.name = "calculator", .description = "math",
                         .risk = COS_TOOL_SAFE,    .cost_eur = 0.0001};
    cos_tool_t fread_ = {.name = "file_read",  .description = "read",
                         .risk = COS_TOOL_READ,    .cost_eur = 0.0001};
    cos_sigma_tool_register(&reg, &calc);
    cos_sigma_tool_register(&reg, &fread_);

    cos_plan_t plan;
    cos_plan_step_t storage[4];
    cos_plan_budget_t b = {
        .max_steps    = 4,
        .max_cost_eur = 0.001,   /* well above 4 × 0.0001 */
        .max_risk     = COS_TOOL_READ,
        .tau_replan   = 0.60f,
    };
    cos_sigma_plan_init(&plan, storage, 4, "demo: list + count + math", b);

    float ss;
    cos_tool_call_t c_list  = cos_sigma_tool_select(&reg, "file_read .", ".", &ss);
    cos_tool_call_t c_calc  = cos_sigma_tool_select(&reg, "calculator sum", "1+1", &ss);

    int e1 = cos_sigma_plan_add_step(&plan, "list directory", c_list,  0.05f);
    int e2 = cos_sigma_plan_add_step(&plan, "count lines",    c_list,  0.08f);
    int e3 = cos_sigma_plan_add_step(&plan, "sum counts",     c_calc,  0.10f);
    (void)e1; (void)e2; (void)e3;

    cos_sigma_agent_t ag; cos_sigma_agent_init(&ag, 0.90f, 0.10f);
    int ctx_counter = 0;
    const char *outs[4] = {0};
    cos_plan_status_t st = cos_sigma_plan_execute(&plan, &ag, demo_exec,
                                                  &ctx_counter, 0, outs);

    printf("{\"kernel\":\"sigma_plan\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"goal\":\"%s\","
             "\"budget\":{\"max_steps\":%d,\"max_cost_eur\":%.4f,"
                         "\"max_risk\":%d,\"tau_replan\":%.4f},"
             "\"n_steps\":%d,\"spent_eur\":%.4f,"
             "\"sigma_plan\":%.4f,\"status\":\"%s\","
             "\"exec_count\":%d,"
             "\"steps\":["
                "{\"desc\":\"%s\",\"sigma_pre\":%.4f,\"sigma_post\":%.4f,\"done\":%s},"
                "{\"desc\":\"%s\",\"sigma_pre\":%.4f,\"sigma_post\":%.4f,\"done\":%s},"
                "{\"desc\":\"%s\",\"sigma_pre\":%.4f,\"sigma_post\":%.4f,\"done\":%s}"
             "]"
           "},"
           "\"pass\":%s}\n",
           rc,
           plan.goal,
           plan.budget.max_steps, plan.budget.max_cost_eur,
           (int)plan.budget.max_risk, (double)plan.budget.tau_replan,
           plan.count, plan.spent_eur,
           (double)plan.sigma_plan, status_name(st),
           ctx_counter,
           plan.steps[0].description, (double)plan.steps[0].sigma_pre,
           (double)plan.steps[0].sigma_post,
           plan.steps[0].completed ? "true" : "false",
           plan.steps[1].description, (double)plan.steps[1].sigma_pre,
           (double)plan.steps[1].sigma_post,
           plan.steps[1].completed ? "true" : "false",
           plan.steps[2].description, (double)plan.steps[2].sigma_pre,
           (double)plan.steps[2].sigma_post,
           plan.steps[2].completed ? "true" : "false",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Plan demo:\n");
        fprintf(stderr, "  goal:   %s\n", plan.goal);
        fprintf(stderr, "  steps:  %d / %d  (σ_plan=%.3f)\n",
                plan.count, plan.budget.max_steps, (double)plan.sigma_plan);
        fprintf(stderr, "  status: %s\n", status_name(st));
    }
    return rc;
}
